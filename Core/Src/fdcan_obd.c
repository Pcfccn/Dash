/* ============================================================================
 *  fdcan_obd.c  — read-only OBD-II poller over FDCAN1 (classic CAN, 500 kbps)
 *
 *  Requests go to the ECM (0x7E0) and the trans controller (0x7E2); responses
 *  arrive on 0x7E8 / 0x7EA. Replies longer than 7 bytes use ISO-TP, so we
 *  answer our own First Frames with a Flow Control (30 00 00) and reassemble
 *  Consecutive Frames. No request in the current schedule needs that, but the
 *  path stays (and is tested) for when one does.
 *
 *  Project note: the vehicle bus is RECEIVED BY POLLING (obd_rx_poll) from the
 *  main loop -- the project has no FDCAN NVIC handler. FDCAN bit timing is set
 *  by MX_FDCAN1_Init (HSE 25 MHz kernel -> 500 kbps), not here.
 * ========================================================================== */
#include "fdcan_obd.h"
#include "main.h"                /* Error_Handler */
#include "cluster_config.h"
#include "can_sniff.h"
#include <string.h>
#include <math.h>

/* Every float starts as NaN = "the bus has never given us this". A PID the
 * calibration does not support simply never answers, and leaving those at 0.0f
 * would paint a plausible lie on the screen (0 V battery, 0 degC oil). The UI
 * renders NaN as "--". */
volatile obd_data_t g_obd = {
    .speed = NAN, .rpm   = NAN, .cool = NAN, .oil = NAN, .iat     = NAN,
    .load  = NAN, .boost = NAN, .rail = NAN, .egt = NAN, .battery = NAN,
    .atf   = NAN, .oil_press = NAN,
    .gear  = -1,  .sel_range = -1, .can_ok = false,
};

static FDCAN_HandleTypeDef *hfd;
static volatile uint32_t    last_rx_ms;

/* ---- ISO-TP reassembly (single flow at a time; OBD is request/response) --- */
static uint8_t  itp_buf[64];
static uint16_t itp_len, itp_got;
static uint8_t  itp_next_seq;
static bool     itp_active;
static uint32_t itp_src_id;          /* CFs must come from the FF's sender      */
/* A transfer with no progress for this many POLL TICKS is abandoned. Ticks,
 * not milliseconds, for the same reason as OBD_AWAIT_TICKS below: CFs can sit
 * in the RX FIFO while a display flush blocks the loop, and a wall-clock check
 * at drain time would throw away a transfer that actually arrived in time. */
#define ITP_TTL_TICKS 4u
static uint8_t  itp_ttl;

/* Frames dropped because their length / PCI did not add up (truncated or
 * malformed). Surfaced on DIAG next to the RX FIFO loss count. */
static volatile uint16_t rx_bad_cnt;
static void rx_bad(void) { if (rx_bad_cnt < 0xFFFFu) rx_bad_cnt++; }

/* PID 0x01 freshness: consecutive PID 0x01 attempts that got no answer — the
 * TX FIFO refused the request, or its transaction timed out. After
 * MIL_MAX_MISS of them in a row MIL/DTC are shown as unknown. Counted in
 * attempts, not milliseconds, so it stays right however slowly the loop runs.
 * Broadcasts keep can_ok alive even when the ECM has stopped answering, so
 * without this a once-seen "MIL OFF" would stay on DIAG indefinitely. */
#define MIL_MAX_MISS 3u
static uint8_t mil_miss;

static void mil_attempt_failed(void) {
    if (mil_miss < 0xFFu) mil_miss++;
    if (mil_miss >= MIL_MAX_MISS && g_obd.mil_valid) {
        g_obd.mil_valid = false;
        obd_on_update();
    }
}

/* ---- the outstanding diagnostic transaction ------------------------------
 * The OBD port is shared: the owner may leave a scan tool / insurance dongle /
 * logger plugged in alongside this cluster. Those also poll, and their
 * multi-frame replies land on the same 0x7E8..0x7EA IDs we listen to. If we
 * answered every First Frame with our own Flow Control, the ECU would receive
 * TWO FC frames for one transfer and both readers would get corrupt data.
 * So we only drive a transfer we actually asked for.
 *
 * At most ONE request is outstanding: every req_*() refuses while
 * await_resp_id != 0, and obd_poll_tick() only ages the open transaction. It
 * is closed by a valid answer, by an NRC for its service (0x78 "response
 * pending" extends it instead), by a refused Flow Control, or by its timeout.
 * A multi-frame answer belongs to the same transaction, so finishing one can
 * only ever close its own slot. (Before, the next tick's request overwrote the
 * slot mid-transfer, and the old transfer's last CF then cleared the new one.)
 * Single-frame replies are still decoded whoever asked: they need no FC and
 * the decoders key off the echoed PID/DID, so another tester's polling feeds
 * our gauges too. */
/* Timeouts are in POLL TICKS, not milliseconds. A wall-clock deadline is
 * unusable here: lv_port_disp's flush is a blocking row-by-row HAL_SPI_Transmit
 * of the whole 320x480 panel, so one superloop iteration can take a few hundred
 * ms. A 50 ms deadline had always expired by the time obd_rx_poll() next ran,
 * which silently killed EVERY multi-frame reply (fast/misc/EGT groups) while
 * single-frame ones kept working -- exactly the "0 everywhere" symptom.
 * obd_rx_poll and obd_poll_tick share that same stalled loop, so counting ticks
 * tracks the real request/response cadence no matter how slow a frame is. */
#define OBD_AWAIT_TICKS    2u   /* normal answer                                */
#define OBD_PENDING_TICKS 40u   /* after NRC 0x78: the ECU promised an answer   */
static uint32_t await_resp_id;  /* 0 = idle, else the response ID we wait for   */
static uint8_t  await_ttl;
static uint8_t  await_sid;      /* service of the outstanding request           */
static uint16_t await_key;      /* its first PID (mode 01) or DID (mode 22)     */

static volatile uint16_t tx_fail_cnt;   /* frames the TX FIFO refused           */

static void txn_close(void) {
    await_resp_id = 0u;
    itp_active    = false;
}

/* The transaction ended without an answer (timeout, refused Flow Control,
 * malformed or unusable reply). */
static void txn_failed(void) {
    if (await_sid == 0x01 && await_key == 0x01) mil_attempt_failed();
    txn_close();
}

/* Is this reply (its first payload bytes) the answer to OUR outstanding
 * request? Matching the response ID alone let another tester's reply from the
 * same ECU clear our slot, or get our Flow Control for its multi-frame
 * transfer. The positive-response SID and the echoed PID/DID must match too
 * (mode 03 echoes nothing); a negative response must name our service. A
 * tester asking the very same thing still collides — unavoidable on a shared
 * bus without seeing the requests. */
static bool reply_is_ours(uint32_t resp_id, const uint8_t *p, uint16_t n) {
    if (await_resp_id == 0u || resp_id != await_resp_id || n < 1u) return false;
    if (p[0] == 0x7F) return n >= 2u && p[1] == await_sid;
    if (p[0] != (uint8_t)(await_sid + 0x40u)) return false;
    switch (await_sid) {
        case 0x01: return n >= 2u && p[1] == (uint8_t)await_key;
        case 0x22: return n >= 3u && ((((uint16_t)p[1]) << 8) | p[2]) == await_key;
        default:   return true;
    }
}

/* =============================== TX ====================================== */
/* Returns false when the TX FIFO refused the frame (full, or the controller is
 * not started, e.g. mid bus-off recovery): the caller must then not wait for a
 * reply that cannot come. */
static bool can_send(uint32_t req_id, const uint8_t *data8) {
    FDCAN_TxHeaderTypeDef tx = {0};
    tx.Identifier          = req_id;
    tx.IdType              = FDCAN_STANDARD_ID;
    tx.TxFrameType         = FDCAN_DATA_FRAME;
    tx.DataLength          = FDCAN_DLC_BYTES_8;
    tx.FDFormat            = FDCAN_CLASSIC_CAN;   /* classic, not FD           */
    tx.BitRateSwitch       = FDCAN_BRS_OFF;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfd, &tx, (uint8_t *)data8) != HAL_OK) {
        if (tx_fail_cnt < 0xFFFFu) tx_fail_cnt++;
        return false;
    }
    return true;
}

/* Send a request and make it the outstanding transaction. Refused (false)
 * while another transaction is open, or when the TX FIFO refuses the frame —
 * then nothing waits for a reply that cannot come. */
static bool request(uint32_t req_id, const uint8_t *d, uint8_t sid, uint16_t key) {
    if (await_resp_id != 0u) return false;
    if (!can_send(req_id, d)) {
        if (sid == 0x01 && key == 0x01) mil_attempt_failed();
        return false;
    }
    await_resp_id = req_id + 8u;     /* 0x7E0->0x7E8, 0x7E2->0x7EA              */
    await_ttl     = OBD_AWAIT_TICKS;
    await_sid     = sid;
    await_key     = key;
    return true;
}

/* mode 01, one or more PIDs in a single frame (GM accepts up to 6) */
static bool req_mode01(const uint8_t *pids, uint8_t n) {
    uint8_t d[8] = {0};
    d[0] = (uint8_t)(1 + n);         /* PCI length: SID + n PIDs               */
    d[1] = 0x01;                     /* service                                */
    for (uint8_t i = 0; i < n && i < 6; i++) d[2 + i] = pids[i];
    return request(OBD_REQ_ECM, d, 0x01, pids[0]);
}

/* mode 22 (UDS ReadDataByIdentifier): request one 2-byte DID from a module.
 * Used for the GM-enhanced values (ATF temp, gear) the trans controller serves
 * on 0x7E2. Responses come back as [0x62][DID_hi][DID_lo][data...]. */
static bool req_mode22(uint32_t req_id, uint16_t did) {
    uint8_t d[8] = {0};
    d[0] = 0x03;                     /* PCI length: SID + 2 DID bytes           */
    d[1] = 0x22;                     /* service                                 */
    d[2] = (uint8_t)(did >> 8);
    d[3] = (uint8_t)(did & 0xFFu);
    return request(req_id, d, 0x22, did);
}

/* Flow Control: clear-to-send, no block, no separation */
static bool send_flow_control(uint32_t resp_id) {
    uint8_t d[8] = { 0x30, 0x00, 0x00, 0,0,0,0,0 };
    return can_send(resp_id - 8u, d); /* request id = response id - 8          */
}

/* =============================== decode ================================== */
static void set_f(volatile float *dst, float v) {
    if (*dst != v) { *dst = v; obd_on_update(); }
}

/* Boost is a GAUGE pressure but PID 0x0B reports ABSOLUTE manifold pressure, so
 * the reading has to be referenced to ambient (PID 0x33). Without that, a
 * stationary engine shows ~1.0 bar of "boost". Either PID can arrive first, so
 * both feed this and the gauge is recomputed whenever one of them lands.
 * baro defaults to sea level: a plausible reference beats no reading at all. */
static float last_map_kpa = NAN;
static float baro_kpa     = 101.0f;

static void update_boost(void) {
    if (isnan(last_map_kpa)) return;
    float bar = (last_map_kpa - baro_kpa) / 100.0f;
    if (bar < 0.0f) bar = 0.0f;      /* vacuum: not meaningful on this gauge   */
    set_f(&g_obd.boost, bar);
}

/* Data bytes after each mode-01 PID we decode (SAE J1979). Any other PID has
 * an unknown length, so nothing after it in the same reply can be located. */
static int8_t mode01_len(uint8_t pid) {
    switch (pid) {
        case 0x04: case 0x05: case 0x0B: case 0x0D:
        case 0x0F: case 0x33: case 0x5C:            return 1;
        case 0x0C: case 0x23: case 0x42:            return 2;
        case 0x01:                                  return 4;
        case 0x78:                                  return 9;
        default:                                    return -1;
    }
}

/* Decode a completed mode-01 payload: [0x41][PID][data]... (concatenated).
 * The whole reply is validated before anything is written: every PID known and
 * complete, and together exactly filling the payload. A truncated reply used to
 * decode its missing bytes as 0 — a believable 0 rpm. An unknown PID (another
 * tester's request) is skipped silently; a short one counts as malformed.
 * Returns true only for a reply that was complete and decoded. */
static bool decode_mode01(const uint8_t *p, uint16_t n) {
    uint16_t i = 1;                  /* skip 0x41                              */
    if (n < 2u) { rx_bad(); return false; }
    while (i < n) {
        int8_t len = mode01_len(p[i]);
        if (len < 0) return false;
        if (i + 1u + (uint16_t)len > n) { rx_bad(); return false; }
        i += 1u + (uint16_t)len;
    }

    for (i = 1; i < n; i += 1u + (uint16_t)mode01_len(p[i])) {
        const uint8_t *v = &p[i + 1];
        switch (p[i]) {
            /* PID 0x0D is A km/h raw, but this ECM reads ~12% optimistic vs GPS
             * (60 GPS = 67 on screen, 2026-08-23 run) — scale to match GPS.     */
            case 0x0D: set_f(&g_obd.speed,   v[0] * (60.0f / 67.0f));          break;
            case 0x0C: set_f(&g_obd.rpm,     ((v[0] * 256) + v[1]) / 4.0f);    break;
            case 0x05: set_f(&g_obd.cool,    v[0] - 40);                       break;
            case 0x5C: set_f(&g_obd.oil,     v[0] - 40);                       break;
            case 0x0F: set_f(&g_obd.iat,     v[0] - 40);                       break;
            case 0x04: set_f(&g_obd.load,    v[0] * 100.0f / 255.0f);          break;
            case 0x0B: last_map_kpa = (float)v[0]; update_boost();             break; /* absolute MAP, kPa */
            case 0x33: baro_kpa     = (float)v[0]; update_boost();             break; /* barometric, kPa   */
            case 0x23: set_f(&g_obd.rail,    ((v[0] * 256) + v[1]) / 10.0f);   break; /* bar */
            /* Exhaust gas temperature, 9 data bytes: v[0] = supported-sensor bit
             * mask, then FOUR 2-byte sensors. Sensor 1 is v[1],v[2] as (x/10)-40.
             * Reading the mask as the high byte desynchronises everything. */
            case 0x78: set_f(&g_obd.egt, (((v[1] * 256) + v[2]) / 10.0f) - 40.0f); break;
            case 0x42: set_f(&g_obd.battery, ((v[0] * 256) + v[1]) / 1000.0f); break;
            case 0x01: g_obd.mil = (v[0] & 0x80) != 0;
                       g_obd.dtc_count = v[0] & 0x7F;
                       g_obd.mil_valid = true; mil_miss = 0; obd_on_update(); break;
            default:                                                       break;
        }
    }
    return true;
}

/* Decode a mode-22 payload: [0x62][DID_hi][DID_lo][data...]. Scaling for the
 * GM-enhanced DIDs is community-sourced and unverified on this car -- if a value
 * looks wrong, the fix is here (the formula), not the request. Each DID is only
 * accepted from the module it is requested from, so another tester's reply from
 * a different module cannot land in the wrong field. Returns true for a
 * well-formed reply (DID + at least one data byte). */
static bool decode_mode22(uint32_t resp_id, const uint8_t *p, uint16_t n) {
    if (n < 4) { rx_bad(); return false; }      /* need SID + DID + >=1 data     */
    uint16_t did = ((uint16_t)p[1] << 8) | p[2];
    uint8_t  A   = p[3];
    switch (did) {
        case 0x1940:                            /* trans fluid (ATF) temp        */
            if (resp_id != OBD_RESP_TCM2) break;
            set_f(&g_obd.atf, (float)A - 40.0f);
            break;
        case 0x1154:                            /* engine oil temp (GM enhanced) */
            if (resp_id != OBD_RESP_ECM) break;
            set_f(&g_obd.oil, (float)A - 40.0f);
            break;
        case 0x199A: {                          /* current gear (raw index in A) */
            if (resp_id != OBD_RESP_TCM2) break;
            /* Keep the raw byte: the DIAG page shows it so a wrong DID (byte
             * never moves while the selector does) can be told apart from a
             * wrong scaling (byte moves, gear label doesn't match). */
            if (g_obd.gear_raw != A) { g_obd.gear_raw = A; obd_on_update(); }
            int8_t g = (int8_t)A;
            if (g_obd.gear != g) { g_obd.gear = g; obd_on_update(); }
            break;
        }
        default: break;
    }
    return true;
}

/* Mode 01 is only ever requested from the ECM (0x7E0), so only its replies are
 * decoded; a TCM answering another tester's functional request with its own
 * idea of e.g. vehicle speed must not overwrite the ECM's value.
 * Returns true when the payload is a well-formed answer of its service — only
 * then may it close our transaction (a malformed reply must not). */
static bool dispatch(uint32_t resp_id, const uint8_t *p, uint16_t n) {
    if (n == 0) return false;
    if (p[0] == 0x41) return resp_id == OBD_RESP_ECM && decode_mode01(p, n);
    if (p[0] == 0x62) return decode_mode22(resp_id, p, n);
    if (p[0] == 0x7F) {
        if (n < 3) { rx_bad(); return false; }
        /* Negative response. Silently dropping these is what makes an unknown
         * DID indistinguishable from a dead module: an NRC proves the module
         * answered and only the identifier was wrong. Surfaced on DIAG. */
        if (g_obd.last_nrc_sid != p[1] || g_obd.last_nrc != p[2]) {
            g_obd.last_nrc_sid = p[1];
            g_obd.last_nrc     = p[2];
            obd_on_update();
        }
        return true;
    }
    return false;                    /* incl. 0x43: mode 03 is not requested     */
}

/* ---- bus health (surfaced on the SNIFF page) -----------------------------
 * The sniffer's promiscuous filter puts the full 500 kbps bus into the same
 * 16-deep RX FIFO the OBD replies use, and a display flush can stall the drain
 * for hundreds of ms -- so the FIFO overflows routinely while sniffing. That is
 * expected and self-heals (blocking mode drops the excess, then resumes on the
 * next drain). What is NOT recoverable on its own is bus-off, and what looks
 * identical to "bus asleep" from the table is "we stopped receiving": this
 * exposes both so a frozen frame counter can be explained rather than guessed.
 * Reads are gated to ~10 Hz; the message-lost flag is a single register bit. */
static volatile uint16_t rx_lost_cnt;   /* RX FIFO0 overflow events            */
static volatile uint16_t busoff_cnt;    /* bus-off recoveries attempted        */
static volatile uint16_t start_fail_cnt; /* HAL_FDCAN_Start refusals (retried) */
static volatile uint8_t  busoff_now, errpass_now;

static void obd_check_health(void) {
    static uint32_t t_last;
    uint32_t now = HAL_GetTick();
    if (now - t_last < 100u) return;
    t_last = now;

    if (__HAL_FDCAN_GET_FLAG(hfd, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST)) {
        __HAL_FDCAN_CLEAR_FLAG(hfd, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
        if (rx_lost_cnt < 0xFFFFu) rx_lost_cnt++;
    }
    FDCAN_ProtocolStatusTypeDef ps;
    if (HAL_FDCAN_GetProtocolStatus(hfd, &ps) == HAL_OK) {
        busoff_now  = ps.BusOff ? 1u : 0u;
        errpass_now = ps.ErrorPassive ? 1u : 0u;
        if (ps.BusOff) {
            /* Bus-off latches the node off the bus until INIT is cleared;
             * Stop then Start does that and begins the recovery sequence.
             * Filters live in message RAM and survive the cycle. */
            if (busoff_cnt < 0xFFFFu) busoff_cnt++;
            (void)HAL_FDCAN_Stop(hfd);
        }
    }
    /* Stopped (the recovery above, or a Start that failed before): start it.
     * A failed Start leaves the controller READY, so this retries on every
     * health check instead of leaving the node off the bus for good. */
    if (HAL_FDCAN_GetState(hfd) == HAL_FDCAN_STATE_READY &&
        HAL_FDCAN_Start(hfd) != HAL_OK && start_fail_cnt < 0xFFFFu)
        start_fail_cnt++;
}

void obd_can_health(obd_health_t *h) {
    h->bus_off        = busoff_now;
    h->err_passive    = errpass_now;
    h->rx_lost        = rx_lost_cnt;
    h->busoff_recover = busoff_cnt;
    h->rx_bad         = rx_bad_cnt;
    h->tx_fail        = tx_fail_cnt;
    h->start_fail     = start_fail_cnt;
}

/* =============================== RX (polled) ============================= */
void obd_rx_poll(void) {
    FDCAN_RxHeaderTypeDef rh;
    /* 64, not 8: HAL_FDCAN_GetRxMessage copies DLCtoBytes[DLC] bytes, and a
     * classic frame may legally carry DLC 9..15 (still 8 data bytes), which the
     * HAL would expand to up to 64 bytes and overrun an 8-byte buffer. */
    uint8_t d[64];
    obd_check_health();
    while (HAL_FDCAN_GetRxFifoFillLevel(hfd, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfd, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;

        /* In this HAL DataLength is the DLC code, equal to the byte count for
         * 0..8 (FDCAN_DLC_BYTES_8 == 8); classic DLC 9..15 means 8 bytes. Every
         * byte read below is checked against dlc. */
        if (rh.IdType != FDCAN_STANDARD_ID || rh.RxFrameType != FDCAN_DATA_FRAME) {
            rx_bad();
            continue;
        }
        uint8_t dlc = (rh.DataLength >= FDCAN_DLC_BYTES_8) ? 8u : (uint8_t)rh.DataLength;

        /* Selector/PRNDL broadcast: a plain 8-byte frame, not ISO-TP. byte 3 is
         * 01 P / 02 R / 03 N / 04 D (see docs/sniff-selector.md). Decoded here,
         * ahead of the OBD-range gate, and it also keeps can_ok alive. */
        if (rh.Identifier == CAN_ID_SELECTOR) {
            if (dlc < 4u) { rx_bad(); continue; }
            last_rx_ms = HAL_GetTick();
            g_obd.can_ok = true;
            int8_t r = (d[3] >= 1 && d[3] <= 4) ? (int8_t)d[3] : -1;
            if (g_obd.sel_range != r) { g_obd.sel_range = r; obd_on_update(); }
            continue;
        }

        /* 0x1BA oil-pressure broadcast candidate #1 — REJECTED (2026-08-23 run).
         * Byte 3 (/100 → bar) does NOT track RPM: it stayed 0.1-0.4 bar at warm
         * idle and 0.2 bar at 1599 rpm / 71 km/h under way, where real oil pressure
         * would be 3-5 bar. No longer fed to oil_press; raw byte still captured for
         * the DIAG side-by-side until candidate #2 is confirmed and this is removed. */
        if (rh.Identifier == CAN_ID_OILP_BCAST) {
            if (dlc < 4u) { rx_bad(); continue; }
            last_rx_ms = HAL_GetTick();
            g_obd.can_ok = true;
            g_obd.oilp_1ba_raw = d[3];
            continue;
        }

        /* 0x0C9 oil-pressure broadcast candidate #2 — UNVERIFIED. Byte 2 spanned
         * 0x27..0xFF on SNIFF and A/36 was only a guess, never calibrated. It is
         * captured raw for the DIAG side-by-side and deliberately does NOT feed
         * oil_press: a guessed pressure with a red low-pressure state is worse
         * than "--". Wire a decode back only after it is checked against an
         * independent gauge (docs/oil-pressure-test.md). */
        if (rh.Identifier == CAN_ID_OILP_CAND2) {
            if (dlc < 3u) { rx_bad(); continue; }
            last_rx_ms = HAL_GetTick();
            g_obd.can_ok = true;
            g_obd.oilp_0c9_raw = d[2];
            continue;
        }

        /* accept ECM 0x7E8, TCM 0x7E9, and the trans controller 0x7EA */
        if (rh.Identifier < OBD_RESP_ECM || rh.Identifier > OBD_RESP_TCM2) {
            /* Everything else only reaches here with the sniffer's wide filter
             * installed; normally the hardware rejects it. */
            can_sniff_feed((uint16_t)rh.Identifier, d, dlc);
            continue;
        }
        if (dlc < 1u) { rx_bad(); continue; }
        last_rx_ms = HAL_GetTick();
        g_obd.can_ok = true;

        uint8_t pci = d[0] >> 4;
        if (pci == 0x0) {                       /* Single Frame                */
            /* SF_DL is 1..7 on classic CAN and must fit in the received bytes;
             * an unchecked 0x0F once read 8 bytes past the frame buffer. */
            uint8_t len = d[0] & 0x0F;
            if (len == 0u || len > 7u || len > dlc - 1u) { rx_bad(); continue; }
            const uint8_t *pl = &d[1];
            bool ours  = reply_is_ours(rh.Identifier, pl, len);
            bool valid = dispatch(rh.Identifier, pl, len);  /* decode even if not ours */
            if (ours && valid) {
                /* NRC 0x78 (response pending) promises the real answer later:
                 * keep the transaction open for it, with a longer bound. */
                if (pl[0] == 0x7F && pl[2] == 0x78) await_ttl = OBD_PENDING_TICKS;
                else                                txn_close();
            }
            /* ours && !valid: a malformed answer leaves the transaction open
             * for a proper one until its timeout. */
        } else if (pci == 0x1) {                /* First Frame                 */
            if (dlc < 8u) { rx_bad(); continue; }
            /* Only drive a multi-frame transfer we requested. Another tester's
             * First Frame must not get our Flow Control (see await_resp_id),
             * and must not clobber our reassembly buffer. */
            if (!reply_is_ours(rh.Identifier, &d[2], 6u)) continue;
            uint16_t len = (uint16_t)(((d[0] & 0x0Fu) << 8) | d[1]);
            /* FF_DL below 8 would have been a Single Frame; above the buffer the
             * transfer could never complete. Reject before sending Flow Control,
             * so the ECU times the transfer out instead of streaming it to us. */
            if (len < 8u || len > sizeof itp_buf) {
                rx_bad();
                txn_failed();
                continue;
            }
            itp_len  = len;
            itp_got  = 0; itp_next_seq = 1; itp_active = true;
            itp_src_id = rh.Identifier;
            itp_ttl  = ITP_TTL_TICKS;
            for (int i = 0; i < 6; i++) itp_buf[itp_got++] = d[2 + i];
            /* No FC on the wire means the ECU never streams the rest. */
            if (!send_flow_control(rh.Identifier)) txn_failed();
        } else if (pci == 0x2 && itp_active && rh.Identifier == itp_src_id) {
            /* Consecutive Frame of our transfer. Every CF but the last carries
             * 7 bytes; the last carries the rest and may be unpadded. A CF short
             * of that — an empty DLC-1 frame included — is malformed and must
             * not advance the sequence. itp_len <= sizeof itp_buf was checked
             * on the First Frame, so need bounds the copy. */
            uint16_t need = (uint16_t)(itp_len - itp_got);
            if (need > 7u) need = 7u;
            if ((d[0] & 0x0F) != itp_next_seq || (uint16_t)(dlc - 1u) < need) {
                rx_bad();
                txn_failed();
                continue;
            }
            itp_next_seq = (itp_next_seq + 1) & 0x0F;
            for (uint16_t i = 0; i < need; i++) itp_buf[itp_got++] = d[1 + i];
            itp_ttl = ITP_TTL_TICKS;
            if (itp_got >= itp_len) {
                /* The transfer belongs to the open transaction, so completing
                 * it closes exactly that one — never a newer request. */
                itp_active = false;
                if (dispatch(itp_src_id, itp_buf, itp_len)) txn_close();
                else                                        txn_failed();
            }
        }
    }
}

/* =============================== init/poll =============================== */
/* Message-RAM layout this module needs: 4 standard filters (set below) and a
 * 16-deep RX FIFO0 of 8-byte elements. fdcan.c and Dash.ioc carry the same
 * values; asserting them here keeps reception working even if a CubeMX
 * regeneration drops them again. That has bitten before: RxFifo0ElmtsNbr = 0
 * meant nothing could be received at all, and with too few filter elements
 * HAL_FDCAN_ConfigFilter does NOT fail (the index check is an assert_param,
 * compiled out) — it silently writes the filter over the next RAM section. */
#define OBD_STD_FILTERS   4u
#define OBD_RX_FIFO0_LEN 16u

/* A config call that fails at init is fatal: there is no CAN without it, and
 * Error_Handler records "HAL ERROR" for DIAG before the watchdog resets. */
static bool cfg_ok(HAL_StatusTypeDef s) {
    if (s == HAL_OK) return true;
    Error_Handler();
    return false;
}

void obd_init(FDCAN_HandleTypeDef *hfdcan) {
    hfd = hfdcan;

    if (hfd->Init.StdFiltersNbr < OBD_STD_FILTERS ||
        hfd->Init.RxFifo0ElmtsNbr < OBD_RX_FIFO0_LEN ||
        hfd->Init.RxFifo0ElmtSize != FDCAN_DATA_BYTES_8) {
        hfd->Init.StdFiltersNbr   = OBD_STD_FILTERS;
        hfd->Init.RxFifo0ElmtsNbr = OBD_RX_FIFO0_LEN;
        hfd->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
        if (!cfg_ok(HAL_FDCAN_Init(hfd))) return;   /* READY: re-lays out RAM */
    }

    /* Accept only the two ECU response IDs into RX FIFO 0. */
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterIndex  = 0;
    f.FilterType   = FDCAN_FILTER_RANGE;         /* accept 0x7E8..0x7EA         */
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = OBD_RESP_ECM;               /* 0x7E8 (range low)           */
    f.FilterID2    = OBD_RESP_TCM2;              /* 0x7EA (range high): ECM,     */
                                                 /* TCM(7E9) and trans(7EA)      */
    if (!cfg_ok(HAL_FDCAN_ConfigFilter(hfd, &f))) return;

    /* Filter 1: the selector/PRNDL broadcast (0x1F5). It is not an OBD
     * request/response, so the narrow OBD filter above would reject it; this
     * exact-match filter lets it into the same FIFO, where obd_rx_poll decodes
     * it specially. (The sniffer's promiscuous filter also covers it, but this
     * is what makes P/R/N/D live during normal operation.) */
    FDCAN_FilterTypeDef fs = {0};
    fs.IdType       = FDCAN_STANDARD_ID;
    fs.FilterIndex  = 1;
    fs.FilterType   = FDCAN_FILTER_MASK;
    fs.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fs.FilterID1    = CAN_ID_SELECTOR;
    fs.FilterID2    = 0x7FFu;                 /* full mask = exact match         */
    if (!cfg_ok(HAL_FDCAN_ConfigFilter(hfd, &fs))) return;

    /* Filter 2: 0x1BA — oil-pressure candidate #1. Mode-22 DIDs 0x115C and
     * 0x1470 return NRC 22/31 on this E98, so oil pressure must be a broadcast
     * the OEM cluster reads passively. Byte 3 was REJECTED on 2026-08-23 (it
     * does not track RPM); the raw byte is still captured for the DIAG
     * side-by-side. TEST SCAFFOLD — remove together with filter 3 once a real
     * source is confirmed. */
    FDCAN_FilterTypeDef fo = {0};
    fo.IdType       = FDCAN_STANDARD_ID;
    fo.FilterIndex  = 2;
    fo.FilterType   = FDCAN_FILTER_MASK;
    fo.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fo.FilterID1    = CAN_ID_OILP_BCAST;
    fo.FilterID2    = 0x7FFu;                   /* exact match                     */
    if (!cfg_ok(HAL_FDCAN_ConfigFilter(hfd, &fo))) return;

    /* Filter 3: 0x0C9 — second oil-pressure candidate under test (2026-07-24).
     * Same rationale as filter 2: capture the frame passively so its byte 2 can
     * be compared against RPM on the DIAG page. TEST SCAFFOLD — remove this
     * filter (and drop StdFiltersNbr back to 3 in fdcan.c) once the real
     * oil-pressure source is settled. 0x0C9 is a fast engine frame, so it adds
     * RX-FIFO pressure; acceptable for a short warm-up/blip test. */
    FDCAN_FilterTypeDef fc = {0};
    fc.IdType       = FDCAN_STANDARD_ID;
    fc.FilterIndex  = 3;
    fc.FilterType   = FDCAN_FILTER_MASK;
    fc.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fc.FilterID1    = CAN_ID_OILP_CAND2;
    fc.FilterID2    = 0x7FFu;                   /* exact match                     */
    if (!cfg_ok(HAL_FDCAN_ConfigFilter(hfd, &fc))) return;

    if (!cfg_ok(HAL_FDCAN_ConfigGlobalFilter(hfd, FDCAN_REJECT, FDCAN_REJECT,
                                             FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE)))
        return;

    if (!cfg_ok(HAL_FDCAN_Start(hfd))) return;
    /* No ActivateNotification: obd_rx_poll() drains the FIFO from the loop. */
}

/* Tiered request scheduler. Call from a 20-50 Hz timer/task.
 *
 * Three priority bands share a single tick counter:
 *
 *   FAST  (default, ~2/3 of ticks): rpm on EVERY fast tick, with MAP and speed
 *         alternating as the 2nd PID (rpm+MAP, then rpm+speed). RPM, MAP (boost)
 *         and speed are the three whose lag is felt; the payload budget is only
 *         7 bytes so rpm+MAP+speed cannot share one single-frame request, hence
 *         the alternation — RPM every fast tick, MAP and speed each every other.
 *         Speed was on MEDIUM before but lagged ~10 s worst-case behind the
 *         blocking flush (2026-08-23 run); on FAST it lands sub-second.
 *
 *   MEDIUM (every MED_DIV ticks): rail+batt, ATF, gear, coolant+IAT, MIL, and
 *         load+baro (fastB). Baro is ambient and barely moves.
 *
 *   SLOW  (every SLOW_DIV ticks): the oil-temp DID (0x1154).
 *
 * Not polled, because this E98 never answers them or nothing uses the answer:
 * PID 0x5C (oil temp: omitted from grouped replies; 0x1154 is the source),
 * PID 0x78 (EGT: no reply), mode 03 (DTC list: never decoded — the count comes
 * from PID 0x01), and the DPF/soot/EGR probes. An unanswered request now holds
 * the one transaction slot for OBD_AWAIT_TICKS, so each of these cost real
 * polling time. Their decoders stay: another tester's replies still feed us.
 *
 * One transaction at a time: while a request is outstanding a tick only ages
 * it (await_ttl, or itp_ttl while its multi-frame answer is arriving), and
 * the band position does not advance. A reply normally lands before the next
 * iteration, so this costs nothing; a request that is never answered (e.g. an
 * unsupported PID) costs OBD_AWAIT_TICKS ticks.
 *
 * Reply budget: every grouped mode-01 request must fit a single ISO-TP frame
 * (≤7 payload bytes: 0x41 echo + PID/data), so it never holds the slot for a
 * multi-frame round trip. */
static struct {                      /* file scope, not function statics, so   */
    uint32_t tick;                   /* the host tests can reset the position  */
    uint8_t  med_idx, fast_tog;
} sched;

void obd_poll_tick(void) {
    static const uint8_t fastRM[] = { 0x0C, 0x0B };       /* rpm(2)+MAP(1)    = 6 payload */
    static const uint8_t fastRS[] = { 0x0C, 0x0D };       /* rpm(2)+speed(1)  = 6 payload */
    static const uint8_t fastB[] = { 0x04, 0x33 };        /* load+baro        = 5 payload */
    static const uint8_t temps[] = { 0x05, 0x0F };        /* cool+iat         = 5 payload */
    static const uint8_t misc[]  = { 0x23, 0x42 };        /* rail(3)+batt(3)  = 7 payload */
    static const uint8_t mil1[]  = { 0x01 };

    /* medium: 6 items, one per MED_DIV ticks */
    #define N_MED    6u
    #define MED_DIV  3u   /* fire medium item every 3 ticks */
    #define SLOW_DIV 16u  /* oil-temp DID every 16 ticks    */

    if (can_sniff_is_active()) return;

    if (await_resp_id != 0u) {
        if (itp_active) {                   /* answer arriving: reassembly clock */
            if (itp_ttl == 0u || --itp_ttl == 0u) txn_failed();
        } else if (await_ttl == 0u || --await_ttl == 0u) {
            txn_failed();
        }
        if (await_resp_id != 0u) return;    /* still outstanding: send nothing   */
    }

    ++sched.tick;

    if (sched.tick % SLOW_DIV == 0u) {
        /* engine oil temp (GM enhanced). Oil PRESSURE is not polled: DIDs
         * 0x115C / 0x1470 return NRC 22/31 on this E98. */
        req_mode22(OBD_REQ_ECM, 0x1154);
    } else if (sched.tick % MED_DIV == 0u) {
        switch (sched.med_idx % N_MED) {
            case 0: req_mode01(misc,  sizeof misc);          break; /* rail, batt       */
            case 1: req_mode22(OBD_REQ_TCM2, 0x1940);       break; /* ATF temp         */
            case 2: req_mode22(OBD_REQ_TCM2, 0x199A);       break; /* gear             */
            case 3: req_mode01(temps, sizeof temps);         break; /* coolant, IAT     */
            case 4: req_mode01(mil1,  sizeof mil1);          break; /* MIL / DTC count  */
            case 5: req_mode01(fastB, sizeof fastB);         break; /* load, baro       */
        }
        sched.med_idx++;
    } else {
        /* fast band: RPM every tick, with MAP and SPEED alternating as the 2nd
         * PID so both stay sub-second even when the display flush stalls the loop.
         * Each request is a single ISO-TP frame (rpm + one PID = 6 payload). */
        if (sched.fast_tog++ & 1u) req_mode01(fastRS, sizeof fastRS);  /* rpm + speed */
        else                       req_mode01(fastRM, sizeof fastRM);  /* rpm + MAP   */
    }
}

void obd_watchdog_tick_1hz(void) {
    if (HAL_GetTick() - last_rx_ms > 1000u) {    /* bus quiet for >1 s          */
        if (g_obd.can_ok) { g_obd.can_ok = false; obd_on_update(); }
    }
}

#if OBD_DEMO
/* Bench demo: cycle a 40 s scripted scenario so every new visual can be checked
 * without a car — normal cruise (green) -> REVERSE (orange border + orange "R")
 * -> PARK -> coolant into the red (full-screen red border). OBD_DEMO must be 0
 * before use in the vehicle (see cluster_config.h). */
void obd_demo_tick(void) {
    uint32_t s = (HAL_GetTick() / 100u) % 400u;          /* 0..399 over 40 s   */

    /* shared "engine warm and healthy" backdrop */
    g_obd.oil = 98;     g_obd.egt = 421;   g_obd.boost = 1.4f;
    g_obd.iat = 45;     g_obd.load = 67;   g_obd.rail = 580;  g_obd.battery = 14.1f;
    g_obd.atf = 82;     g_obd.oil_press = 3.6f;
    g_obd.mil = false;  g_obd.dtc_count = 0;  g_obd.mil_valid = true;
    g_obd.can_ok = true;

    if (s < 120u) {                 /* 0-12 s: DRIVE cruise, all nominal green   */
        g_obd.sel_range = 4; g_obd.gear = 6;
        g_obd.speed = 95;    g_obd.rpm = 2150;  g_obd.cool = 88;
    } else if (s < 200u) {          /* 12-20 s: REVERSE -> orange border + "R"   */
        g_obd.sel_range = 2; g_obd.gear = -1;
        g_obd.speed = 4;     g_obd.rpm = 820;   g_obd.cool = 88;
    } else if (s < 260u) {          /* 20-26 s: PARK                             */
        g_obd.sel_range = 1; g_obd.gear = -1;
        g_obd.speed = 0;     g_obd.rpm = 760;   g_obd.cool = 89;
    } else {                        /* 26-40 s: coolant climbs into crit -> red  */
        uint32_t t = s - 260u;      /* 0..139 */
        g_obd.sel_range = 4; g_obd.gear = 5;
        g_obd.speed = 110;   g_obd.rpm = 2600;
        g_obd.cool = (float)(88 + (int)(t * 20u / 140u));  /* 88 -> 108, crosses 93/97 */
    }
    obd_on_update();
}
#endif
