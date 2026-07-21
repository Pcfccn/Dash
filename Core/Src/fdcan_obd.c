/* ============================================================================
 *  fdcan_obd.c  — read-only OBD-II poller over FDCAN1 (classic CAN, 500 kbps)
 *
 *  Requests go to the ECM (0x7E0) / TCM (0x7E1); responses arrive on
 *  0x7E8 / 0x7E9. Multi-byte responses (mode 03 DTC list, multi-PID) use
 *  ISO-TP, so we answer First Frames with a Flow Control (30 00 00) and
 *  reassemble Consecutive Frames.
 *
 *  Project note: the vehicle bus is RECEIVED BY POLLING (obd_rx_poll) from the
 *  main loop -- the project has no FDCAN NVIC handler. FDCAN bit timing is set
 *  by MX_FDCAN1_Init (HSE 25 MHz kernel -> 500 kbps), not here.
 * ========================================================================== */
#include "fdcan_obd.h"
#include "cluster_config.h"
#include "can_sniff.h"
#include <string.h>
#include <math.h>

/* Every float starts as NaN = "the bus has never given us this". A PID the
 * calibration does not support simply never answers, and leaving those at 0.0f
 * would paint a plausible lie on the screen (0 V battery, 0 degC oil). The UI
 * renders NaN as "--". */
volatile obd_data_t g_obd = {
    .speed = NAN, .rpm   = NAN, .cool   = NAN, .oil = NAN, .iat     = NAN,
    .load  = NAN, .boost = NAN, .rail   = NAN, .egt = NAN, .battery = NAN,
    .atf   = NAN, .soot  = NAN, .dpf_dp = NAN, .egr_t = NAN, .since_regen = NAN,
    .oil_press = NAN,
    .gear  = -1,  .sel_range = -1, .can_ok = false,
};

static FDCAN_HandleTypeDef *hfd;
static volatile uint32_t    last_rx_ms;

/* ---- ISO-TP reassembly (single flow at a time; OBD is request/response) --- */
static uint8_t  itp_buf[64];
static uint16_t itp_len, itp_got;
static uint8_t  itp_next_seq;
static bool     itp_active;

/* ---- outstanding-request gate -------------------------------------------
 * The OBD port is shared: the owner may leave a scan tool / insurance dongle /
 * logger plugged in alongside this cluster. Those also poll, and their
 * multi-frame replies land on the same 0x7E8..0x7EA IDs we listen to. If we
 * answered every First Frame with our own Flow Control, the ECU would receive
 * TWO FC frames for one transfer and both readers would get corrupt data.
 * So we only drive a transfer we actually asked for. Exactly one request is
 * outstanding at a time (obd_poll_tick sends one per 25 ms tick), so a single
 * slot is enough. Single-frame replies are still decoded unconditionally —
 * they need no FC, cost nothing, and decode_mode01 keys off the echoed PID,
 * so the other tester's polling transparently feeds our gauges too. */
/* The slot expires in POLL TICKS, not milliseconds. A wall-clock deadline is
 * unusable here: lv_port_disp's flush is a blocking row-by-row HAL_SPI_Transmit
 * of the whole 320x480 panel, so one superloop iteration can take a few hundred
 * ms. A 50 ms deadline had always expired by the time obd_rx_poll() next ran,
 * which silently killed EVERY multi-frame reply (fast/misc/EGT groups) while
 * single-frame ones kept working -- exactly the "0 everywhere" symptom.
 * obd_rx_poll and obd_poll_tick share that same stalled loop, so counting ticks
 * tracks the real request/response cadence no matter how slow a frame is. */
#define OBD_AWAIT_TICKS 2u
static uint32_t await_resp_id;       /* 0 = nothing outstanding                 */
static uint8_t  await_ttl;

static void expect_reply(uint32_t req_id) {
    await_resp_id = req_id + 8u;     /* 0x7E0->0x7E8, 0x7E1->0x7E9, 0x7E2->0x7EA */
    await_ttl     = OBD_AWAIT_TICKS;
}

static bool reply_is_ours(uint32_t resp_id) {
    return await_resp_id != 0u && await_resp_id == resp_id;
}

/* =============================== TX ====================================== */
static void can_send(uint32_t req_id, const uint8_t *data8) {
    FDCAN_TxHeaderTypeDef tx = {0};
    tx.Identifier          = req_id;
    tx.IdType              = FDCAN_STANDARD_ID;
    tx.TxFrameType         = FDCAN_DATA_FRAME;
    tx.DataLength          = FDCAN_DLC_BYTES_8;
    tx.FDFormat            = FDCAN_CLASSIC_CAN;   /* classic, not FD           */
    tx.BitRateSwitch       = FDCAN_BRS_OFF;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    HAL_FDCAN_AddMessageToTxFifoQ(hfd, &tx, (uint8_t *)data8);
}

/* mode 01, one or more PIDs in a single frame (GM accepts up to 6) */
static void req_mode01(const uint8_t *pids, uint8_t n) {
    uint8_t d[8] = {0};
    d[0] = (uint8_t)(1 + n);         /* PCI length: SID + n PIDs               */
    d[1] = 0x01;                     /* service                                */
    for (uint8_t i = 0; i < n && i < 6; i++) d[2 + i] = pids[i];
    expect_reply(OBD_REQ_ECM);
    can_send(OBD_REQ_ECM, d);
}

/* mode 03: request stored DTCs */
static void req_mode03(void) {
    uint8_t d[8] = { 0x01, 0x03, 0,0,0,0,0,0 };
    expect_reply(OBD_REQ_ECM);
    can_send(OBD_REQ_ECM, d);
}

/* mode 22 (UDS ReadDataByIdentifier): request one 2-byte DID from a module.
 * Used for the GM-enhanced values (ATF temp, gear) the trans controller serves
 * on 0x7E2. Responses come back as [0x62][DID_hi][DID_lo][data...]. */
static void req_mode22(uint32_t req_id, uint16_t did) {
    uint8_t d[8] = {0};
    d[0] = 0x03;                     /* PCI length: SID + 2 DID bytes           */
    d[1] = 0x22;                     /* service                                 */
    d[2] = (uint8_t)(did >> 8);
    d[3] = (uint8_t)(did & 0xFFu);
    expect_reply(req_id);
    can_send(req_id, d);
}

/* Flow Control: clear-to-send, no block, no separation */
static void send_flow_control(uint32_t resp_id) {
    uint8_t d[8] = { 0x30, 0x00, 0x00, 0,0,0,0,0 };
    can_send(resp_id - 8u, d);       /* request id = response id - 8           */
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

/* Decode a completed mode-01 payload: [0x41][PID][A][B]... (concatenated) */
static void decode_mode01(const uint8_t *p, uint16_t n) {
    uint16_t i = 1;                  /* skip 0x41                              */
    while (i < n) {
        uint8_t pid = p[i++];
        uint8_t A = (i < n) ? p[i] : 0;
        uint8_t B = (i + 1 < n) ? p[i + 1] : 0;
        uint8_t C = (i + 2 < n) ? p[i + 2] : 0;
        switch (pid) {
            case 0x0D: set_f(&g_obd.speed,   A);                        i += 1; break;
            case 0x0C: set_f(&g_obd.rpm,     ((A * 256) + B) / 4.0f);   i += 2; break;
            case 0x05: set_f(&g_obd.cool,    A - 40);                   i += 1; break;
            case 0x5C: set_f(&g_obd.oil,     A - 40);                   i += 1; break;
            case 0x0F: set_f(&g_obd.iat,     A - 40);                   i += 1; break;
            case 0x04: set_f(&g_obd.load,    A * 100.0f / 255.0f);      i += 1; break;
            case 0x0B: last_map_kpa = (float)A; update_boost();          i += 1; break; /* absolute MAP, kPa */
            case 0x33: baro_kpa     = (float)A; update_boost();          i += 1; break; /* barometric, kPa   */
            case 0x23: set_f(&g_obd.rail,    ((A * 256) + B) * 10 / 1000.0f); i += 2; break; /* MPa */
            /* Exhaust gas temperature, 9 data bytes: A = supported-sensor bit
             * mask, then FOUR 2-byte sensors. Sensor 1 is B,C as (x/10)-40.
             * Decoding A,B as the value (and stepping 2) reads the mask as the
             * high byte and desynchronises everything after it. */
            case 0x78: set_f(&g_obd.egt, (((B * 256) + C) / 10.0f) - 40.0f); i += 9; break;
            case 0x42: set_f(&g_obd.battery, ((A * 256) + B) / 1000.0f);i += 2; break;
            /* Standard J1979 diesel EGR temperature: A = supported-sensor bits,
             * B = EGR temp sensor 1 (bank1) as B-40 degC. 5 data bytes total
             * (A + up to 4 sensors); we take sensor 1 and skip the rest.
             * MUST be requested in its own frame (see obd_poll_tick). */
            case 0x6B: set_f(&g_obd.egr_t, (float)B - 40.0f);           i += 5; break;
            /* Diesel aftertreatment probes with no verified scaling on this ECM
             * (DPF differential pressure, DPF temp, aftertreatment status, PM).
             * Each is requested on its own so this is the only PID in the frame:
             * capture the raw bytes for DIAG and stop. If the ECM does not
             * support one, it simply never reaches here. */
            case 0x7A: case 0x7C: case 0x8B: case 0x86: {
                g_obd.probe_pid = pid;
                uint8_t m = 0;
                for (; m < 6 && (i + m) < n; m++) g_obd.probe_raw[m] = p[i + m];
                g_obd.probe_len = m;
                obd_on_update();
                i = n;                       /* dedicated single-PID response   */
                break;
            }
            case 0x01: g_obd.mil = (A & 0x80) != 0;
                       g_obd.dtc_count = A & 0x7F; obd_on_update();     i += 4; break;
            default:   i += 2; break; /* unknown: assume 2 data bytes          */
        }
    }
}

/* Decode a mode-22 payload: [0x62][DID_hi][DID_lo][data...]. Scaling for the
 * GM-enhanced DIDs is community-sourced and unverified on this car -- if a value
 * looks wrong, the fix is here (the formula), not the request. */
static void decode_mode22(const uint8_t *p, uint16_t n) {
    if (n < 4) return;                          /* need SID + DID + >=1 data     */
    uint16_t did = ((uint16_t)p[1] << 8) | p[2];
    uint8_t  A   = p[3];
    switch (did) {
        case 0x1940:                            /* trans fluid (ATF) temp        */
            set_f(&g_obd.atf, (float)A - 40.0f);
            break;
        case 0x1154:                            /* engine oil temp (GM enhanced) */
            set_f(&g_obd.oil, (float)A - 40.0f);
            break;
        case 0x1470:                            /* engine oil pressure           */
            /* Community scaling A*(116/256) psi; shown in bar. GM DID/scaling
             * varies by ECM, so the raw byte is surfaced on DIAG to calibrate
             * against a known idle pressure before trusting the number. */
            g_obd.oil_press_raw = A;
            set_f(&g_obd.oil_press,
                  (float)A * (116.0f / 256.0f) / 14.5038f);   /* psi -> bar      */
            break;
        /* GM diesel DPF DIDs, from the Opel/Vauxhall Astra-K set (same GM
         * Global-B diesel family as this E98). Verified on the car by whether
         * they answer; scaling from that community and checked against known
         * conditions (dP ~0 at idle, soot a low %). See docs/enhanced-dids.md. */
        case 0x336A:                            /* DPF soot accumulation, %      */
            set_f(&g_obd.soot, (float)A * 100.0f / 255.0f);
            break;
        case 0x3039: {                          /* distance since last regen, km */
            /* The E98 answers this DID but reports 0xFFFF when it has no value
             * to give (KOEO / no regen recorded). Treat that sentinel as "no
             * data" rather than painting a literal 65535 km. */
            uint16_t km = ((uint16_t)A << 8) | p[4];
            if (km != 0xFFFF)
                set_f(&g_obd.since_regen, (float)km);
            break;
        }
        case 0x20F4:                            /* DPF differential pressure, kPa */
            set_f(&g_obd.dpf_dp, (float)(int8_t)A);   /* signed byte             */
            break;
        case 0x20F6: {                          /* DPF regeneration status       */
            bool r = (A & 0x01u) != 0u;
            if (g_obd.regen_active != r) { g_obd.regen_active = r; obd_on_update(); }
            break;
        }
        case 0x199A: {                          /* current gear (raw index in A) */
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
}

static void dispatch(const uint8_t *p, uint16_t n) {
    if (n == 0) return;
    if (p[0] == 0x41) decode_mode01(p, n);
    else if (p[0] == 0x62) decode_mode22(p, n);
    else if (p[0] == 0x7F && n >= 3) {
        /* Negative response. Silently dropping these is what makes an unknown
         * DID indistinguishable from a dead module: an NRC proves the module
         * answered and only the identifier was wrong. Surfaced on DIAG. */
        if (g_obd.last_nrc_sid != p[1] || g_obd.last_nrc != p[2]) {
            g_obd.last_nrc_sid = p[1];
            g_obd.last_nrc     = p[2];
            obd_on_update();
        }
    }
    /* 0x43 (DTC list) intentionally ignored */
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
            HAL_FDCAN_Stop(hfd);
            HAL_FDCAN_Start(hfd);
        }
    }
}

void obd_can_health(bool *bus_off, bool *err_passive,
                    uint16_t *lost, uint16_t *recover) {
    if (bus_off)     *bus_off     = busoff_now;
    if (err_passive) *err_passive = errpass_now;
    if (lost)        *lost        = rx_lost_cnt;
    if (recover)     *recover     = busoff_cnt;
}

/* =============================== RX (polled) ============================= */
void obd_rx_poll(void) {
    FDCAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    obd_check_health();
    while (HAL_FDCAN_GetRxFifoFillLevel(hfd, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfd, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;

        /* Selector/PRNDL broadcast: a plain 8-byte frame, not ISO-TP. byte 3 is
         * 01 P / 02 R / 03 N / 04 D (see docs/sniff-selector.md). Decoded here,
         * ahead of the OBD-range gate, and it also keeps can_ok alive. */
        if (rh.Identifier == CAN_ID_SELECTOR) {
            last_rx_ms = HAL_GetTick();
            g_obd.can_ok = true;
            int8_t r = (d[3] >= 1 && d[3] <= 4) ? (int8_t)d[3] : -1;
            if (g_obd.sel_range != r) { g_obd.sel_range = r; obd_on_update(); }
            continue;
        }

        /* accept ECM 0x7E8, TCM 0x7E9, and the trans controller 0x7EA */
        if (rh.Identifier < OBD_RESP_ECM || rh.Identifier > OBD_RESP_TCM2) {
            /* Everything else only reaches here with the sniffer's wide filter
             * installed; normally the hardware rejects it. */
            can_sniff_feed((uint16_t)rh.Identifier, d,
                           (uint8_t)(rh.DataLength > 8u ? 8u : rh.DataLength));
            continue;
        }
        last_rx_ms = HAL_GetTick();
        g_obd.can_ok = true;

        uint8_t pci = d[0] >> 4;
        if (pci == 0x0) {                       /* Single Frame                */
            uint8_t len = d[0] & 0x0F;
            if (reply_is_ours(rh.Identifier)) await_resp_id = 0;  /* satisfied  */
            dispatch(&d[1], len);               /* decode even if not ours     */
        } else if (pci == 0x1) {                /* First Frame                 */
            /* Only drive a multi-frame transfer we requested. Another tester's
             * First Frame must not get our Flow Control (see await_resp_id),
             * and must not clobber our reassembly buffer. */
            if (!reply_is_ours(rh.Identifier)) continue;
            itp_len  = ((d[0] & 0x0F) << 8) | d[1];
            itp_got  = 0; itp_next_seq = 1; itp_active = true;
            for (int i = 0; i < 6 && itp_got < itp_len; i++) itp_buf[itp_got++] = d[2 + i];
            send_flow_control(rh.Identifier);
        } else if (pci == 0x2 && itp_active) {  /* Consecutive Frame           */
            if ((d[0] & 0x0F) == itp_next_seq) {
                itp_next_seq = (itp_next_seq + 1) & 0x0F;
                for (int i = 0; i < 7 && itp_got < itp_len && itp_got < sizeof(itp_buf); i++)
                    itp_buf[itp_got++] = d[1 + i];
                if (itp_got >= itp_len) {
                    itp_active = false; await_resp_id = 0;
                    dispatch(itp_buf, itp_len);
                }
            } else { itp_active = false; }      /* sequence error -> drop       */
        }
    }
}

/* =============================== init/poll =============================== */
void obd_init(FDCAN_HandleTypeDef *hfdcan) {
    hfd = hfdcan;

    /* Accept only the two ECU response IDs into RX FIFO 0.
     * (MX_FDCAN1_Init must allocate >=1 std filter + a non-zero RX FIFO0.) */
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterIndex  = 0;
    f.FilterType   = FDCAN_FILTER_RANGE;         /* accept 0x7E8..0x7EA         */
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = OBD_RESP_ECM;               /* 0x7E8 (range low)           */
    f.FilterID2    = OBD_RESP_TCM2;              /* 0x7EA (range high): ECM,     */
                                                 /* TCM(7E9) and trans(7EA)      */
    HAL_FDCAN_ConfigFilter(hfd, &f);

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
    HAL_FDCAN_ConfigFilter(hfd, &fs);

    HAL_FDCAN_ConfigGlobalFilter(hfd, FDCAN_REJECT, FDCAN_REJECT,
                                 FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);

    HAL_FDCAN_Start(hfd);
    /* No ActivateNotification: obd_rx_poll() drains the FIFO from the loop. */
}

/* Round-robin request scheduler. Call from a 20-50 Hz timer/task. */
void obd_poll_tick(void) {
    static uint8_t step = 0;
    /* IMPORTANT: keep every grouped mode-01 request small enough that its reply
     * fits a SINGLE ISO-TP frame (<=7 payload bytes: the 0x41 echo + PID/data).
     * This E98 answers single-frame grouped requests reliably but its multi-frame
     * responses do not survive the polled RX across the blocking display flush
     * (rpm/speed/load/MAP/rail/batt all read "--" when packed into one 4-PID
     * request whose 10-14 byte reply needs a First Frame + Flow Control + CF).
     * So the old {0C,0D,04,0B} and {23,42,33,01} groups are split into halves,
     * each of which is a single frame. A 1-byte PID costs 2 bytes, a 2-byte PID
     * costs 3; budget is 6 after the 0x41. */
    static const uint8_t fastA[] = { 0x0C, 0x0D };        /* rpm(3)+speed(2) = 6      */
    static const uint8_t fastB[] = { 0x04, 0x0B, 0x33 };  /* load+MAP+baro   = 6      */
    static const uint8_t temps[] = { 0x05, 0x5C, 0x0F };  /* cool+oil+iat   <= 7      */
    static const uint8_t misc[]  = { 0x23, 0x42 };        /* rail(3)+batt(3) = 6      */
    static const uint8_t mil1[]  = { 0x01 };              /* MIL/monitor    = 5       */
    static uint8_t probe = 0;
    /* Enhanced ENGINE DIDs (mode 22 → ECM 0x7E0). Oil temp/pressure from the GM
     * Colorado community; the DPF set from the Opel/Vauxhall Astra-K (same GM
     * Global-B diesel family). These are decoded in decode_mode22; which ones
     * this E98 actually supports is confirmed on the car. */
    static const uint16_t probe_did[] = {
        0x1154,  /* oil temperature         */
        0x1470,  /* oil pressure            */
        0x336A,  /* DPF soot %              */
        0x3039,  /* distance since regen    */
        0x20F4,  /* DPF differential press  */
        0x20F6,  /* DPF regen status        */
    };
    #define N_DID (sizeof probe_did / sizeof probe_did[0])
    /* Standard diesel PIDs (mode 01), one per request so an unsupported wide PID
     * cannot desync another's parse. Unknown ones are captured raw for DIAG. */
    static const uint8_t probe_pid[] = {
        0x78,  /* EGT bank 1              */
        0x6B,  /* EGR temperature         */
        0x7A,  /* DPF differential press  */
        0x7C,  /* DPF temperature         */
        0x8B,  /* diesel aftertreatment   */
        0x86,  /* particulate matter      */
    };
    #define N_PID (sizeof probe_pid)
    #define N_PROBE (N_DID + N_PID)

    /* While sniffing, stay off the bus entirely: our own request/response pairs
     * would otherwise show up as "changing bytes" and compete for the RX FIFO
     * with the broadcast traffic we are trying to catch. */
    if (can_sniff_is_active()) return;

    /* Age the outstanding-request slot before issuing a new one, so a request
     * that never gets answered cannot leave us answering a stranger's First
     * Frame with our Flow Control forever. */
    if (await_ttl && --await_ttl == 0u) await_resp_id = 0u;

    /* fastA (rpm/speed) is hit twice per cycle for a livelier tach/speedo;
     * enhanced (mode 22, GM-specific) and the DTC scan are interleaved. ATF temp
     * + gear go to the trans controller on 0x7E2; if they never populate, that
     * module/DID is wrong for this truck. Every mode-01 group here is a single
     * frame (see the note above the arrays). */
    switch (step) {
        case 0: req_mode01(fastA, sizeof fastA);  break; /* rpm, speed             */
        case 1: req_mode22(OBD_REQ_TCM2, 0x1940); break; /* ATF / trans fluid temp */
        case 2: req_mode01(fastB, sizeof fastB);  break; /* load, MAP, baro        */
        case 3: req_mode22(OBD_REQ_TCM2, 0x199A); break; /* current gear           */
        case 4: req_mode01(temps, sizeof temps);  break; /* cool, oil, iat         */
        case 5: req_mode01(fastA, sizeof fastA);  break; /* rpm, speed (2nd hit)   */
        case 6: req_mode01(misc,  sizeof misc);   break; /* rail, batt             */
        case 7: /* rotate one enhanced/diesel probe per cycle: first the mode-22
                 * engine DIDs, then the standard mode-01 diesel PIDs. Which ones
                 * actually answer is read off the DPF tiles and the DIAG line. */
                if (probe < N_DID) req_mode22(OBD_REQ_ECM, probe_did[probe]);
                else               req_mode01(&probe_pid[probe - N_DID], 1);
                probe = (uint8_t)((probe + 1) % N_PROBE);
                break;
        case 8: req_mode01(mil1,  sizeof mil1);   break; /* MIL / monitor status   */
        case 9: req_mode03();                     break; /* DTC list ~ once/cycle  */
    }
    step = (step + 1) % 10;
}

void obd_watchdog_tick_1hz(void) {
    if (HAL_GetTick() - last_rx_ms > 1000u) {    /* bus quiet for >1 s          */
        if (g_obd.can_ok) { g_obd.can_ok = false; obd_on_update(); }
    }
}

#if OBD_DEMO
/* Bench demo: fill g_obd with the reference "cruise" scenario, and sweep the
 * coolant 85..100..85 (~30 s) so the 93/97 warn/crit thresholds are visible. */
void obd_demo_tick(void) {
    uint32_t s = (HAL_GetTick() / 100u) % 300u;          /* 0..299 over 30 s   */
    /* sweep coolant 40..100..40 so every band shows: <50 cold-blue, ok green,
     * >=93 warn amber, >=97 crit red */
    int cool = (s < 150) ? 40 + (int)(s * 60 / 150)
                         : 40 + (int)((299 - s) * 60 / 150);

    g_obd.speed = 95;   g_obd.rpm = 2150;  g_obd.cool = (float)cool;
    g_obd.oil = 98;     g_obd.egt = 421;   g_obd.boost = 1.4f;
    g_obd.iat = 45;     g_obd.load = 67;   g_obd.rail = 58;   g_obd.battery = 14.1f;
    g_obd.atf = 82;     g_obd.soot = 42;   g_obd.dpf_dp = 3.1f;
    g_obd.since_regen = 180; g_obd.egr_t = 96;   g_obd.gear = 6;
    g_obd.mil = false;  g_obd.dtc_count = 0;
    g_obd.can_ok = true;
    obd_on_update();
}
#endif
