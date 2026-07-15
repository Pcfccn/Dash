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

volatile obd_data_t g_obd = { .can_ok = false };

static FDCAN_HandleTypeDef *hfd;
static volatile uint32_t    last_rx_ms;

/* ---- ISO-TP reassembly (single flow at a time; OBD is request/response) --- */
static uint8_t  itp_buf[64];
static uint16_t itp_len, itp_got;
static uint8_t  itp_next_seq;
static bool     itp_active;

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
    can_send(OBD_REQ_ECM, d);
}

/* mode 03: request stored DTCs */
static void req_mode03(void) {
    uint8_t d[8] = { 0x01, 0x03, 0,0,0,0,0,0 };
    can_send(OBD_REQ_ECM, d);
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
            case 0x01: g_obd.mil = (A & 0x80) != 0;
                       g_obd.dtc_count = A & 0x7F; obd_on_update();     i += 4; break;
            default:   i += 2; break; /* unknown: assume 2 data bytes          */
        }
    }
}

static void dispatch(const uint8_t *p, uint16_t n) {
    if (n == 0) return;
    if (p[0] == 0x41) decode_mode01(p, n);
    /* p[0] == 0x43 (mode 03 DTC list) can be decoded here later */
}

/* =============================== RX (polled) ============================= */
void obd_rx_poll(void) {
    FDCAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    while (HAL_FDCAN_GetRxFifoFillLevel(hfd, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfd, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
        if (rh.Identifier != OBD_RESP_ECM && rh.Identifier != OBD_RESP_TCM) continue;
        last_rx_ms = HAL_GetTick();
        g_obd.can_ok = true;

        uint8_t pci = d[0] >> 4;
        if (pci == 0x0) {                       /* Single Frame                */
            uint8_t len = d[0] & 0x0F;
            dispatch(&d[1], len);
        } else if (pci == 0x1) {                /* First Frame                 */
            itp_len  = ((d[0] & 0x0F) << 8) | d[1];
            itp_got  = 0; itp_next_seq = 1; itp_active = true;
            for (int i = 0; i < 6 && itp_got < itp_len; i++) itp_buf[itp_got++] = d[2 + i];
            send_flow_control(rh.Identifier);
        } else if (pci == 0x2 && itp_active) {  /* Consecutive Frame           */
            if ((d[0] & 0x0F) == itp_next_seq) {
                itp_next_seq = (itp_next_seq + 1) & 0x0F;
                for (int i = 0; i < 7 && itp_got < itp_len && itp_got < sizeof(itp_buf); i++)
                    itp_buf[itp_got++] = d[1 + i];
                if (itp_got >= itp_len) { itp_active = false; dispatch(itp_buf, itp_len); }
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
    f.FilterType   = FDCAN_FILTER_DUAL;          /* two exact IDs               */
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = OBD_RESP_ECM;               /* 0x7E8                       */
    f.FilterID2    = OBD_RESP_TCM;               /* 0x7E9                       */
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

    switch (step) {
        case 0: req_mode01(fast,  sizeof fast);  break;
        case 1: req_mode01(temps, sizeof temps); break;
        case 2: req_mode01(misc,  sizeof misc);  break;
        case 3: req_mode03();                    break; /* DTC list ~ once/cycle */
    }
    step = (step + 1) % 4;
}

void obd_watchdog_tick_1hz(void) {
    if (HAL_GetTick() - last_rx_ms > 1000u) {    /* bus quiet for >1 s          */
        if (g_obd.can_ok) { g_obd.can_ok = false; obd_on_update(); }
    }
}
