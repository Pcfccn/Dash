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
#include <string.h>

volatile obd_data_t g_obd = { .can_ok = false, .gear = -1 };

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
#define OBD_AWAIT_MS 50u             /* replies land in a few ms; poll is 25 ms */
static uint32_t await_resp_id;       /* 0 = nothing outstanding                 */
static uint32_t await_until_ms;

static void expect_reply(uint32_t req_id) {
    await_resp_id  = req_id + 8u;    /* 0x7E0->0x7E8, 0x7E1->0x7E9, 0x7E2->0x7EA */
    await_until_ms = HAL_GetTick() + OBD_AWAIT_MS;
}

static bool reply_is_ours(uint32_t resp_id) {
    return await_resp_id == resp_id &&
           (int32_t)(HAL_GetTick() - await_until_ms) < 0;   /* tick-wrap safe */
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

/* Decode a completed mode-01 payload: [0x41][PID][A][B]... (concatenated) */
static void decode_mode01(const uint8_t *p, uint16_t n) {
    uint16_t i = 1;                  /* skip 0x41                              */
    while (i < n) {
        uint8_t pid = p[i++];
        uint8_t A = (i < n) ? p[i] : 0;
        uint8_t B = (i + 1 < n) ? p[i + 1] : 0;
        switch (pid) {
            case 0x0D: set_f(&g_obd.speed,   A);                        i += 1; break;
            case 0x0C: set_f(&g_obd.rpm,     ((A * 256) + B) / 4.0f);   i += 2; break;
            case 0x05: set_f(&g_obd.cool,    A - 40);                   i += 1; break;
            case 0x5C: set_f(&g_obd.oil,     A - 40);                   i += 1; break;
            case 0x0F: set_f(&g_obd.iat,     A - 40);                   i += 1; break;
            case 0x04: set_f(&g_obd.load,    A * 100.0f / 255.0f);      i += 1; break;
            case 0x0B: set_f(&g_obd.boost,   A / 100.0f);               i += 1; break; /* MAP; subtract baro(0x33) for gauge */
            case 0x23: set_f(&g_obd.rail,    ((A * 256) + B) * 10 / 1000.0f); i += 2; break; /* MPa */
            case 0x78: set_f(&g_obd.egt,     ((A * 256 + B) / 10.0f) - 40); i += 2; break;
            case 0x42: set_f(&g_obd.battery, ((A * 256) + B) / 1000.0f);i += 2; break;
            /* Standard J1979 diesel EGR temperature: A = supported-sensor bits,
             * B = EGR temp sensor 1 (bank1) as B-40 degC. 5 data bytes total
             * (A + up to 4 sensors); we take sensor 1 and skip the rest.
             * MUST be requested in its own frame (see obd_poll_tick). */
            case 0x6B: set_f(&g_obd.egr_t, (float)B - 40.0f);           i += 5; break;
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
        case 0x199A: {                          /* current gear (raw index in A) */
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
    /* 0x43 (DTC list) / 0x7F (negative response) intentionally ignored */
}

/* =============================== RX (polled) ============================= */
void obd_rx_poll(void) {
    FDCAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    while (HAL_FDCAN_GetRxFifoFillLevel(hfd, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfd, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
        /* accept ECM 0x7E8, TCM 0x7E9, and the trans controller 0x7EA */
        if (rh.Identifier < OBD_RESP_ECM || rh.Identifier > OBD_RESP_TCM2) continue;
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
    HAL_FDCAN_ConfigGlobalFilter(hfd, FDCAN_REJECT, FDCAN_REJECT,
                                 FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);

    HAL_FDCAN_Start(hfd);
    /* No ActivateNotification: obd_rx_poll() drains the FIFO from the loop. */
}

/* Round-robin request scheduler. Call from a 20-50 Hz timer/task. */
void obd_poll_tick(void) {
    static uint8_t step = 0;
    static const uint8_t fast[]  = { 0x0C, 0x0D, 0x04, 0x0B };     /* rpm,speed,load,MAP */
    static const uint8_t temps[] = { 0x05, 0x5C, 0x0F, 0x78 };     /* cool,oil,iat,egt   */
    static const uint8_t misc[]  = { 0x23, 0x42, 0x33, 0x01 };     /* rail,batt,baro,mil */
    static const uint8_t egr[]   = { 0x6B };  /* EGR temp: own frame (multi-byte) */

    /* fast group is hit twice per cycle; enhanced (mode 22, GM-specific) and the
     * DTC scan are interleaved. ATF temp + gear go to the trans controller on
     * 0x7E2; if they never populate, that module/DID is wrong for this truck. */
    switch (step) {
        case 0: req_mode01(fast,  sizeof fast);   break;
        case 1: req_mode22(OBD_REQ_TCM2, 0x1940); break; /* ATF / trans fluid temp */
        case 2: req_mode01(temps, sizeof temps);  break;
        case 3: req_mode22(OBD_REQ_TCM2, 0x199A); break; /* current gear           */
        case 4: req_mode01(fast,  sizeof fast);   break;
        case 5: req_mode01(misc,  sizeof misc);   break;
        case 6: req_mode01(egr,   sizeof egr);    break; /* EGR temperature        */
        case 7: req_mode03();                     break; /* DTC list ~ once/cycle  */
    }
    step = (step + 1) % 8;
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
