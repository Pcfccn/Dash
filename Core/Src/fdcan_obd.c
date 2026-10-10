/* ============================================================================
 *  fdcan_obd.c  — read-only OBD-II poller over FDCAN1 (classic CAN, 500 kbps)
 *
 *  Requests go to the ECM (0x7E0) and the trans controller (0x7E2); responses
 *  arrive on 0x7E8 / 0x7EA. Replies longer than 7 bytes use ISO-TP, so we
 *  answer our own First Frames with a Flow Control (30 00 00) and reassemble
 *  Consecutive Frames. No request in the current schedule needs that, but the
 *  path stays (and is tested) for when one does. Oil pressure may come as a
 *  GMLAN data packet on 0x5E8 instead (see "oil pressure" below).
 *
 *  RX: the FDCAN interrupt moves frames into a ring and obd_rx_poll() decodes
 *  them from the main loop (see "RX" below). FDCAN bit timing is set by
 *  MX_FDCAN1_Init (HSE 25 MHz kernel -> 500 kbps), not here.
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

/* ---- RX ring between the FDCAN interrupt and the loop (see "RX" below) --- */
/* 256 x 28 B = 7 KB. A 200 ms render stall with a busy bus (e.g. SNIFF's wide
 * filter at ~1000 frames/s) is ~200 frames: 64 would overflow, 256 holds it
 * plus the 16-deep hardware FIFO. Overflow is still counted (LOST on DIAG),
 * and the deepest the ring has been is kept (Q on DIAG) so the margin is
 * measured rather than guessed. */
#define RXQ_LEN 256u                         /* power of two                    */
typedef struct {
    uint32_t id, idtype, ftype, dlc;         /* raw header fields               */
    uint32_t rx_ms;                          /* HAL tick when it was received   */
    uint8_t  data[8];
} rxq_t;
static rxq_t             rxq[RXQ_LEN];
static volatile uint32_t rxq_head, rxq_tail;
static volatile uint16_t rxq_drop;           /* ring full: frame dropped        */
static volatile uint16_t rxq_hwm;            /* most entries ever waiting       */
static bool              rx_irq;             /* RX interrupt active             */

/* Receive time of the frame being decoded. Freshness stamps use this, not the
 * time it is decoded: a frame that waited in the ring through a 2 s render
 * stall is 2 s old, and stamping it "now" made a stale RPM look fresh. */
static uint32_t rx_ms_cur;

/* ---- ISO-TP reassembly (single flow at a time; OBD is request/response) --- */
static uint8_t  itp_buf[64];
static uint16_t itp_len, itp_got;
static uint8_t  itp_next_seq;
static bool     itp_active;
static uint32_t itp_src_id;          /* CFs must come from the FF's sender      */
/* A transfer with no FF/CF progress for this long is abandoned (ISO 15765-2
 * N_Cr is 1 s). See the deadline note below for why milliseconds are safe. */
#define ITP_TIMEOUT_MS 1000u
static uint32_t itp_deadline;

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

/* ---- oil pressure: GM PID 0xA22C ------------------------------------------
 * J1979 has no oil-pressure PID, and this E98 refuses the petrol-GM DIDs
 * 0x115C / 0x1470 (NRC 31). The diesel ECM's own sensor value is PID 0xA22C,
 * one byte, A x 4 kPa: ScanGauge's X-Gauge for the LWN 2.8 Duramax (and the
 * Cruze diesel) reads it as "07E0 2C FE A22C", MTH 29/50 psi = 4 kPa/count.
 * That is the GMLAN way, not $22: $2C (DynamicallyDefineMessage) makes data
 * packet 0xFE carry PID 0xA22C, $AA 01 FE (ReadDataByPacketIdentifier, send
 * once) asks for the packet, and it arrives as a UUDT frame on 0x5E8:
 * [FE][A]... — no ISO-TP PCI, no SID. NRCs still come on 0x7E8.
 * Plain $22 A22C is tried first (this ECM answers $22 for other GM PIDs, e.g.
 * 0x1154); only a refusal switches to $2C/$AA.
 * $2C is the one service here that is not a pure read: it tells the ECM which
 * parameter to pack into a diagnostic packet. The definition lives in ECM RAM
 * and is gone at its next reset; no calibration, memory or actuator is touched,
 * and it is what GM scan tools do for live data. Nothing is ever cleared. */
#define OILP_MISS22_MAX 3u      /* unanswered $22 A22C before trying $2C/$AA   */
static struct {                 /* file scope: the host tests reset it          */
    uint8_t mode;               /* obd_oilp_mode_t                              */
    uint8_t miss22;
    uint8_t nrc22, nrc2c, nrcaa;
    bool    have_raw;
    uint8_t raw;
} oilp;

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
/* Deadlines are in milliseconds and are only looked at in obd_poll_tick(),
 * which must run after obd_rx_poll() in the same iteration (cluster_app_run
 * does exactly that). The loop can still stall, but every reply that arrived
 * before the check has been captured by the RX interrupt into the ring (or is
 * still in the FIFO in the polled fallback) and is processed first, so a stall
 * cannot make an answered request look timed out. They used to be poll ticks:
 * with RX drained only from the loop behind a blocking full-screen flush, a
 * 50 ms deadline had always expired by the time obd_rx_poll() ran, silently
 * killing every multi-frame reply. */
#define OBD_AWAIT_MS    150u    /* normal answer (OBD P2 is 50 ms)              */
#define OBD_PENDING_MS 5000u    /* after NRC 0x78: UDS P2* limit                */
static uint32_t await_resp_id;  /* 0 = idle, else the response ID we wait for   */
static uint32_t await_deadline; /* HAL tick at which the transaction fails      */
static uint8_t  await_sid;      /* service of the outstanding request           */
static uint16_t await_key;      /* its first PID (mode 01) or DID (mode 22)     */

static volatile uint16_t tx_fail_cnt;   /* frames the TX FIFO refused           */

static void txn_close(void) {
    await_resp_id = 0u;
    itp_active    = false;
}

/* Oil-pressure path, after an NRC for the open transaction. Busy (0x21) and
 * conditions-not-correct (0x22) are retried; any other refusal of $22 moves
 * on to $2C/$AA, and a refusal of $2C ends the search. A refused $AA means the
 * packet is gone (ECM reset) — define it again. */
static void oilp_refused(uint8_t nrc) {
    bool retry = (nrc == 0x21u || nrc == 0x22u);
    switch (await_sid) {
        case 0x22:
            if (await_key != OILP_PID) break;
            oilp.nrc22 = nrc;
            if (!retry) oilp.mode = OILP_DEFINE;
            break;
        case 0x2C: oilp.nrc2c = nrc; if (!retry) oilp.mode = OILP_NONE; break;
        case 0xAA: oilp.nrcaa = nrc; oilp.mode = OILP_DEFINE;          break;
        default: break;
    }
}

/* ... and after no answer at all. An ECM that ignores $22 A22C three times is
 * treated like one that refuses it; a lost $AA answer re-defines the packet. */
static void oilp_failed(void) {
    if (await_sid == 0x22 && await_key == OILP_PID) {
        if (++oilp.miss22 >= OILP_MISS22_MAX) oilp.mode = OILP_DEFINE;
    } else if (await_sid == 0xAA) {
        oilp.mode = OILP_DEFINE;
    }
}

/* The transaction ended without an answer (timeout, refused Flow Control,
 * malformed or unusable reply). */
static void txn_failed(void) {
    if (await_sid == 0x01 && await_key == 0x01) mil_attempt_failed();
    oilp_failed();
    txn_close();
}

/* The open transaction got its answer: p is the validated reply payload. */
static void txn_answered(const uint8_t *p) {
    if (p[0] == 0x7F)          oilp_refused(p[2]);
    else if (await_sid == 0x2C) oilp.mode = OILP_READ;   /* packet defined     */
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
        case 0x2C: return n >= 2u && p[1] == (uint8_t)await_key;   /* DPID echo */
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
    await_deadline = HAL_GetTick() + OBD_AWAIT_MS;
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

/* Oil pressure, one step of whichever path is current (see "oil pressure"
 * above). $2C: [2C][DPID][PID hi][PID lo] -> [6C][DPID]. $AA: [AA][01 = send
 * once][DPID] -> UUDT [DPID][A] on 0x5E8. Returns false when there is nothing
 * to ask (no source). */
static bool req_oilp(void) {
    uint8_t d[8] = {0};
    switch (oilp.mode) {
        case OILP_VIA22:
            return req_mode22(OBD_REQ_ECM, OILP_PID);
        case OILP_DEFINE:
            d[0] = 0x04; d[1] = 0x2C; d[2] = OILP_DPID;
            d[3] = (uint8_t)(OILP_PID >> 8); d[4] = (uint8_t)(OILP_PID & 0xFFu);
            return request(OBD_REQ_ECM, d, 0x2C, OILP_DPID);
        case OILP_READ:
            d[0] = 0x03; d[1] = 0xAA; d[2] = 0x01; d[3] = OILP_DPID;
            return request(OBD_REQ_ECM, d, 0xAA, OILP_DPID);
        default:
            return false;
    }
}

/* Flow Control: clear-to-send, no block, no separation */
static bool send_flow_control(uint32_t resp_id) {
    uint8_t d[8] = { 0x30, 0x00, 0x00, 0,0,0,0,0 };
    return can_send(resp_id - 8u, d); /* request id = response id - 8          */
}

/* =============================== decode ================================== */
/* Store a decoded value and stamp it fresh — also when it did not change, so a
 * steady reading is not mistaken for a stale one. */
static void set_m(metric_key_t k, volatile float *dst, float v) {
    g_obd.upd_ms[k] = rx_ms_cur;
    if (*dst != v) { *dst = v; obd_on_update(); }
}

bool obd_is_fresh(const volatile obd_data_t *d, metric_key_t k, uint32_t now) {
    uint16_t lim = metric_stale_ms[k];
    return lim == 0u || (now - d->upd_ms[k]) <= lim;
}

bool obd_sel_fresh(const volatile obd_data_t *d, uint32_t now) {
    return (now - d->sel_upd_ms) <= SEL_STALE_MS;
}

static void oilp_value(uint8_t a) {
    oilp.raw = a; oilp.have_raw = true; oilp.miss22 = 0;
    set_m(M_OILP, &g_obd.oil_press, (float)a * 0.04f);   /* 4 kPa = 0.04 bar */
}

void obd_oilp_probe(obd_oilp_probe_t *p) {
    p->mode  = oilp.mode;
    p->nrc22 = oilp.nrc22; p->nrc2c = oilp.nrc2c; p->nrcaa = oilp.nrcaa;
    p->have_raw = oilp.have_raw; p->raw = oilp.raw;
}

/* Boost is a GAUGE pressure but PID 0x0B reports ABSOLUTE manifold pressure, so
 * the reading has to be referenced to ambient (PID 0x33). Without that, a
 * stationary engine shows ~1.0 bar of "boost". Either PID can arrive first, so
 * both feed this and the gauge is recomputed whenever one of them lands.
 * No gauge until a real baro has been read: an assumed sea-level 101 kPa
 * would be off by ~0.1 bar per 900 m of altitude, silently. Freshness follows
 * MAP only — baro barely moves, and a fresh baro must not make a stale MAP
 * look current. */
static float    last_map_kpa = NAN;
static float    baro_kpa     = NAN;
static uint32_t map_upd_ms;

static void update_boost(void) {
    if (isnan(last_map_kpa) || isnan(baro_kpa)) return;
    float bar = (last_map_kpa - baro_kpa) / 100.0f;
    if (bar < 0.0f) bar = 0.0f;      /* vacuum: not meaningful on this gauge   */
    if (g_obd.boost != bar) { g_obd.boost = bar; obd_on_update(); }
    g_obd.upd_ms[M_BOOST] = map_upd_ms;
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
            case 0x0D: set_m(M_SPEED, &g_obd.speed,   v[0] * (60.0f / 67.0f));          break;
            case 0x0C: set_m(M_RPM, &g_obd.rpm,     ((v[0] * 256) + v[1]) / 4.0f);    break;
            case 0x05: set_m(M_COOL, &g_obd.cool,    v[0] - 40);                       break;
            case 0x5C: set_m(M_OIL, &g_obd.oil,     v[0] - 40);                       break;
            case 0x0F: set_m(M_IAT, &g_obd.iat,     v[0] - 40);                       break;
            case 0x04: set_m(M_LOAD, &g_obd.load,    v[0] * 100.0f / 255.0f);          break;
            case 0x0B: last_map_kpa = (float)v[0]; map_upd_ms = rx_ms_cur;
                       update_boost();                                     break; /* absolute MAP, kPa */
            case 0x33: baro_kpa     = (float)v[0]; update_boost();             break; /* barometric, kPa   */
            case 0x23: set_m(M_RAIL, &g_obd.rail,    ((v[0] * 256) + v[1]) / 10.0f);   break; /* bar */
            /* Exhaust gas temperature, 9 data bytes: v[0] = supported-sensor bit
             * mask, then FOUR 2-byte sensors. Sensor 1 is v[1],v[2] as (x/10)-40.
             * Reading the mask as the high byte desynchronises everything. */
            case 0x78: set_m(M_EGT, &g_obd.egt, (((v[1] * 256) + v[2]) / 10.0f) - 40.0f); break;
            case 0x42: set_m(M_BATTERY, &g_obd.battery, ((v[0] * 256) + v[1]) / 1000.0f); break;
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
            set_m(M_ATF, &g_obd.atf, (float)A - 40.0f);
            break;
        case 0x1154:                            /* engine oil temp (GM enhanced) */
            if (resp_id != OBD_RESP_ECM) break;
            set_m(M_OIL, &g_obd.oil, (float)A - 40.0f);
            break;
        case OILP_PID:                          /* engine oil pressure, A x 4 kPa */
            if (resp_id != OBD_RESP_ECM) break;
            oilp_value(A);
            break;
        case 0x199A: {                          /* current gear (raw index in A) */
            if (resp_id != OBD_RESP_TCM2) break;
            /* Keep the raw byte: the DIAG page shows it so a wrong DID (byte
             * never moves while the selector does) can be told apart from a
             * wrong scaling (byte moves, gear label doesn't match). */
            g_obd.upd_ms[M_GEAR] = rx_ms_cur;
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
    if (p[0] == 0x6C) {                  /* $2C: packet defined, nothing to decode */
        if (n < 2) { rx_bad(); return false; }
        return true;
    }
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
    /* Stop/Start below change the handle the RX interrupt also uses. */
    if (rx_irq) HAL_NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
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
    if (rx_irq) HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

void obd_can_health(obd_health_t *h) {
    h->bus_off        = busoff_now;
    h->err_passive    = errpass_now;
    uint32_t lost     = (uint32_t)rx_lost_cnt + rxq_drop;   /* FIFO + ring */
    h->rx_lost        = (uint16_t)(lost > 0xFFFFu ? 0xFFFFu : lost);
    h->rx_hwm         = rxq_hwm;
    h->rx_irq         = rx_irq;
    h->busoff_recover = busoff_cnt;
    h->rx_bad         = rx_bad_cnt;
    h->tx_fail        = tx_fail_cnt;
    h->start_fail     = start_fail_cnt;
}

/* =============================== RX ====================================== */
/* Frames are taken out of the 16-deep hardware FIFO by the FDCAN interrupt and
 * parked in this ring; obd_rx_poll() does all the checking and decoding from
 * the loop. The loop can stall (rendering, a slow flush) without the FIFO
 * overflowing, and every reply waiting here has really arrived — which is what
 * makes millisecond deadlines safe again. Single producer (ISR) / single
 * consumer (loop): the ISR only advances rxq_head, the loop only rxq_tail.
 * If the interrupt cannot be enabled, obd_rx_poll() pumps the FIFO itself, the
 * old polled behaviour. */
/* (The ring itself is declared at the top of the file: health code uses it.) */

/* Move everything the controller holds into the ring. Copy only. */
static void rx_pump(void) {
    FDCAN_RxHeaderTypeDef rh;
    /* 64, not 8: HAL_FDCAN_GetRxMessage copies DLCtoBytes[DLC] bytes, and a
     * classic frame may legally carry DLC 9..15 (still 8 data bytes), which the
     * HAL would expand to up to 64 bytes and overrun an 8-byte buffer. */
    uint8_t d[64];
    while (HAL_FDCAN_GetRxFifoFillLevel(hfd, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfd, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
        uint32_t head = rxq_head;
        if (head - rxq_tail >= RXQ_LEN) {
            if (rxq_drop < 0xFFFFu) rxq_drop++;
            continue;
        }
        rxq_t *e = &rxq[head & (RXQ_LEN - 1u)];
        e->id = rh.Identifier; e->idtype = rh.IdType;
        e->ftype = rh.RxFrameType; e->dlc = rh.DataLength;
        e->rx_ms = HAL_GetTick();            /* ISR time (polled: pump time)   */
        memcpy(e->data, d, sizeof e->data);
        __DMB();                             /* entry before the new head       */
        rxq_head = head + 1u;
        uint32_t occ = head + 1u - rxq_tail;
        if (occ > rxq_hwm) rxq_hwm = (uint16_t)occ;
    }
}

static bool rxq_pop(rxq_t *out) {
    uint32_t tail = rxq_tail;
    if (tail == rxq_head) return false;
    __DMB();                                 /* head before the entry it covers */
    *out = rxq[tail & (RXQ_LEN - 1u)];
    __DMB();
    rxq_tail = tail + 1u;
    return true;
}

/* Defined here, not in stm32h7xx_it.c: enabling the FDCAN1 interrupt in
 * CubeMX would generate a second definition — a deliberately loud link error. */
void FDCAN1_IT0_IRQHandler(void) {
    HAL_FDCAN_IRQHandler(hfd);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *h, uint32_t its) {
    (void)its;
    if (h == hfd) rx_pump();
}

void obd_rx_poll(void) {
    obd_check_health();
    if (!rx_irq) rx_pump();                  /* fallback: no RX interrupt       */

    rxq_t e;
    while (rxq_pop(&e)) {
        FDCAN_RxHeaderTypeDef rh = {0};
        rh.Identifier  = e.id;
        rh.IdType      = e.idtype;
        rh.RxFrameType = e.ftype;
        rh.DataLength  = e.dlc;
        const uint8_t *d = e.data;
        rx_ms_cur = e.rx_ms;

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
            last_rx_ms = rx_ms_cur;
            g_obd.can_ok = true;
            int8_t r = (d[3] >= 1 && d[3] <= 4) ? (int8_t)d[3] : -1;
            g_obd.sel_upd_ms = rx_ms_cur;
            if (g_obd.sel_range != r) { g_obd.sel_range = r; obd_on_update(); }
            continue;
        }

        /* GMLAN UUDT packet from the ECM: [DPID][data...], a plain frame (no
         * ISO-TP PCI, no SID). Only the answer to our own $AA read is decoded:
         * another tester may have defined the same DPID number with different
         * contents. */
        if (rh.Identifier == OBD_UUDT_ECM) {
            if (dlc < 2u) { rx_bad(); continue; }
            last_rx_ms = rx_ms_cur;
            g_obd.can_ok = true;
            if (await_resp_id != 0u && await_sid == 0xAA && d[0] == (uint8_t)await_key) {
                oilp_value(d[1]);
                txn_close();
            }
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
        last_rx_ms = rx_ms_cur;
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
                if (pl[0] == 0x7F && pl[2] == 0x78) await_deadline = HAL_GetTick() + OBD_PENDING_MS;
                else                                txn_answered(pl);
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
            itp_deadline = HAL_GetTick() + ITP_TIMEOUT_MS;
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
            itp_deadline = HAL_GetTick() + ITP_TIMEOUT_MS;
            if (itp_got >= itp_len) {
                /* The transfer belongs to the open transaction, so completing
                 * it closes exactly that one — never a newer request. */
                itp_active = false;
                if (dispatch(itp_src_id, itp_buf, itp_len)) txn_answered(itp_buf);
                else                                        txn_failed();
            }
        }
    }
}

/* =============================== init/poll =============================== */
/* Message-RAM layout this module needs: 3 standard filters (set below) and a
 * 16-deep RX FIFO0 of 8-byte elements. fdcan.c and Dash.ioc carry at least
 * these values (4 filters: one spare, left disabled by HAL_FDCAN_Init's RAM
 * clear); asserting them here keeps reception working even if a CubeMX
 * regeneration drops them again. That has bitten before: RxFifo0ElmtsNbr = 0
 * meant nothing could be received at all, and with too few filter elements
 * HAL_FDCAN_ConfigFilter does NOT fail (the index check is an assert_param,
 * compiled out) — it silently writes the filter over the next RAM section. */
#define OBD_STD_FILTERS   3u
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

    /* Filter 2: the ECM's UUDT diagnostic packets (0x5E8), where the answer
     * to the $AA oil-pressure read arrives. Silent unless something asks:
     * the ECM sends these only on request. */
    FDCAN_FilterTypeDef fu = {0};
    fu.IdType       = FDCAN_STANDARD_ID;
    fu.FilterIndex  = 2;
    fu.FilterType   = FDCAN_FILTER_MASK;
    fu.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fu.FilterID1    = OBD_UUDT_ECM;
    fu.FilterID2    = 0x7FFu;                   /* exact match                     */
    if (!cfg_ok(HAL_FDCAN_ConfigFilter(hfd, &fu))) return;

    if (!cfg_ok(HAL_FDCAN_ConfigGlobalFilter(hfd, FDCAN_REJECT, FDCAN_REJECT,
                                             FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE)))
        return;

    if (!cfg_ok(HAL_FDCAN_Start(hfd))) return;

    /* RX by interrupt (line 0, the HAL default). Priority 6: below the RTOS
     * syscall ceiling, though the ISR makes no RTOS calls. If the notification
     * cannot be enabled, obd_rx_poll() keeps draining the FIFO from the loop. */
    rx_irq = false;
    if (HAL_FDCAN_ActivateNotification(hfd, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) == HAL_OK) {
        HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
        rx_irq = true;
    }
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
 *   MEDIUM (every MED_DIV ticks): rail+batt, ATF, gear, coolant+IAT, MIL,
 *         load+baro (fastB), and oil pressure (one step of its $22 or $2C/$AA
 *         path; a fast request instead once it has no source). Baro is
 *         ambient and barely moves.
 *
 *   SLOW  (every SLOW_DIV ticks): the oil-temp DID (0x1154).
 *
 * Not polled, because this E98 never answers them or nothing uses the answer:
 * PID 0x5C (oil temp: omitted from grouped replies; 0x1154 is the source),
 * PID 0x78 (EGT: no reply), mode 03 (DTC list: never decoded — the count comes
 * from PID 0x01), and the DPF/soot/EGR probes. An unanswered request now holds
 * the one transaction slot for OBD_AWAIT_MS, so each of these cost real
 * polling time. Their decoders stay: another tester's replies still feed us.
 *
 * One transaction at a time: while a request is outstanding a tick only checks
 * its deadline (await_deadline, or itp_deadline while its multi-frame answer
 * is arriving), and the band position does not advance. A reply normally lands
 * before the next iteration, so this costs nothing; a request that is never
 * answered (e.g. an unsupported PID) costs OBD_AWAIT_MS.
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

    /* medium: 7 items, one per MED_DIV ticks */
    #define N_MED    7u
    #define MED_DIV  3u   /* fire medium item every 3 ticks */
    #define SLOW_DIV 16u  /* oil-temp DID every 16 ticks    */

    if (can_sniff_is_active()) return;

    if (await_resp_id != 0u) {
        uint32_t deadline = itp_active ? itp_deadline : await_deadline;
        if ((int32_t)(HAL_GetTick() - deadline) >= 0) txn_failed();
        if (await_resp_id != 0u) return;    /* still outstanding: send nothing   */
    }

    ++sched.tick;

    bool fast = false;
    if (sched.tick % SLOW_DIV == 0u) {
        req_mode22(OBD_REQ_ECM, 0x1154);                    /* engine oil temp      */
    } else if (sched.tick % MED_DIV == 0u) {
        switch (sched.med_idx % N_MED) {
            case 0: req_mode01(misc,  sizeof misc);          break; /* rail, batt       */
            case 1: req_mode22(OBD_REQ_TCM2, 0x1940);       break; /* ATF temp         */
            case 2: req_mode22(OBD_REQ_TCM2, 0x199A);       break; /* gear             */
            case 3: req_mode01(temps, sizeof temps);         break; /* coolant, IAT     */
            case 4: req_mode01(mil1,  sizeof mil1);          break; /* MIL / DTC count  */
            case 5: req_mode01(fastB, sizeof fastB);         break; /* load, baro       */
            case 6: fast = !req_oilp();                     break; /* oil pressure     */
        }
        sched.med_idx++;
    } else {
        fast = true;
    }
    if (fast) {
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
    uint32_t now = HAL_GetTick();          /* synthetic values are always fresh */
    for (int k = 0; k < M_COUNT; k++) g_obd.upd_ms[k] = now;
    g_obd.sel_upd_ms = now;
    obd_on_update();
}
#endif
