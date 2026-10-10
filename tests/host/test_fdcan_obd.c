/* Host-side tests for Core/Src/fdcan_obd.c: frame validation, ISO-TP, reply
 * ownership, TX errors and MIL freshness. The real source is #included so the
 * tests can see its static state; the HAL is mocked in mock/stm32h7xx_hal.h.
 *
 * From the repo root, with any host C compiler (here: zig cc), e.g.
 *   python -m ziglang cc -std=gnu11 -g -O1 -Wall -Wextra \
 *       -fsanitize=undefined -fsanitize-trap=undefined \
 *       -Itests/host/mock -ICore/Inc tests/host/test_fdcan_obd.c \
 *       -o build/host/test_fdcan_obd.exe
 * On Linux add -fsanitize=address for out-of-bounds detection.
 *
 * This checks the protocol logic only — not the STM32 FDCAN, the bus or LVGL. */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../Core/Src/fdcan_obd.c"

static int checks, fails;                /* harness counters (see below) */

/* ------------------------------------------------------------- mock HAL */
static uint32_t now_ms;
uint32_t HAL_GetTick(void) { return now_ms; }

typedef struct { uint32_t id, idtype, ftype, dlc; uint8_t data[64]; } rxf_t;
static rxf_t hwq[64];
static int   hw_head, hw_tail;

typedef struct { uint32_t id; uint8_t data[8]; } txf_t;
static txf_t             txlog[64];
static int               tx_n;
static HAL_StatusTypeDef tx_status;

static const uint8_t DLC2B[16] = { 0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64 };

static HAL_StatusTypeDef filter_status, start_status;
static int init_calls, error_calls, start_calls;

void Error_Handler(void) { error_calls++; }      /* firmware: record + hang */

static HAL_StatusTypeDef notif_status;              /* RX IRQ enable result     */
static bool nvic_on;
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *h, uint32_t its, uint32_t bufs)
{ (void)h; (void)its; (void)bufs; return notif_status; }
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t pre, uint32_t sub) { (void)irq; (void)pre; (void)sub; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq)  { (void)irq; nvic_on = true; }
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { (void)irq; nvic_on = false; }
/* The interrupt: what the real HAL_FDCAN_IRQHandler ends up calling. */
void HAL_FDCAN_IRQHandler(FDCAN_HandleTypeDef *h) { HAL_FDCAN_RxFifo0Callback(h, FDCAN_IT_RX_FIFO0_NEW_MESSAGE); }

HAL_StatusTypeDef HAL_FDCAN_Init(FDCAN_HandleTypeDef *h)
{ init_calls++; h->State = HAL_FDCAN_STATE_READY; return HAL_OK; }
HAL_FDCAN_StateTypeDef HAL_FDCAN_GetState(const FDCAN_HandleTypeDef *h) { return h->State; }
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *h, FDCAN_FilterTypeDef *f)
{ (void)h; (void)f; return filter_status; }
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *h, uint32_t a, uint32_t b,
                                               uint32_t c, uint32_t d)
{ (void)h; (void)a; (void)b; (void)c; (void)d; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h)
{
    start_calls++;
    if (start_status != HAL_OK) return start_status;
    h->State = HAL_FDCAN_STATE_BUSY;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef *h) { h->State = HAL_FDCAN_STATE_READY; return HAL_OK; }

HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *h,
                                                FDCAN_TxHeaderTypeDef *tx, uint8_t *data)
{
    (void)h;
#if !OILP_DPID_ENABLE
    /* Built with the data-packet path off: $2C / $AA must never go out. */
    if (data[1] == 0x2C || data[1] == 0xAA) {
        fails++;
        printf("  FAIL: SID %02X transmitted with OILP_DPID_ENABLE 0\n", data[1]);
    }
#endif
    if (tx_status != HAL_OK) return tx_status;
    if (tx_n < 64) {
        txlog[tx_n].id = tx->Identifier;
        memcpy(txlog[tx_n].data, data, 8);
        tx_n++;
    }
    return HAL_OK;
}

uint32_t HAL_FDCAN_GetRxFifoFillLevel(FDCAN_HandleTypeDef *h, uint32_t fifo)
{ (void)h; (void)fifo; return (uint32_t)(hw_tail - hw_head); }

/* Copies DLCtoBytes[DLC] bytes, exactly like the real HAL — up to 64. */
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h, uint32_t loc,
                                         FDCAN_RxHeaderTypeDef *rh, uint8_t *data)
{
    (void)h; (void)loc;
    if (hw_head == hw_tail) return HAL_ERROR;
    const rxf_t *f = &hwq[hw_head++];
    memset(rh, 0, sizeof *rh);
    rh->Identifier  = f->id;
    rh->IdType      = f->idtype;
    rh->RxFrameType = f->ftype;
    rh->DataLength  = f->dlc;
    memcpy(data, f->data, DLC2B[f->dlc & 15u]);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef *h, FDCAN_ProtocolStatusTypeDef *ps)
{ (void)h; memset(ps, 0, sizeof *ps); return HAL_OK; }

/* ------------------------------------------- stubs for the rest of the app */
static int  updates;
static bool sniff_on;
void obd_on_update(void) { updates++; }
void can_sniff_feed(uint16_t id, const uint8_t *d, uint8_t len) { (void)id; (void)d; (void)len; }
bool can_sniff_is_active(void) { return sniff_on; }

/* ------------------------------------------------------------- harness */
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void push_frame(uint32_t id, uint32_t idtype, uint32_t dlc, const uint8_t *b, int n)
{
    if (hw_head == hw_tail) hw_head = hw_tail = 0;   /* drained: restart the queue */
    if (hw_tail >= (int)(sizeof hwq / sizeof hwq[0])) { printf("  rx queue overflow\n"); fails++; return; }
    rxf_t *f = &hwq[hw_tail++];
    f->id = id; f->idtype = idtype; f->ftype = FDCAN_DATA_FRAME; f->dlc = dlc;
    memset(f->data, 0xEE, sizeof f->data);           /* poison past the given bytes */
    memcpy(f->data, b, (size_t)n);
}
#define PUSH(id, dlc, ...) do { const uint8_t b_[] = { __VA_ARGS__ }; \
    push_frame((id), FDCAN_STANDARD_ID, (dlc), b_, (int)sizeof b_); } while (0)

static FDCAN_HandleTypeDef hmock;

static void reset(void)
{
    hw_head = hw_tail = 0; tx_n = 0; tx_status = HAL_OK; updates = 0; now_ms = 1000;
    sniff_on = false;
    memset(&sched, 0, sizeof sched);              /* scheduler position */
    await_sid = 0; await_key = 0;
    memset((void *)&g_obd, 0, sizeof g_obd);
    g_obd.speed = NAN; g_obd.rpm = NAN; g_obd.cool = NAN; g_obd.oil = NAN; g_obd.iat = NAN;
    g_obd.load = NAN; g_obd.boost = NAN; g_obd.rail = NAN; g_obd.egt = NAN; g_obd.battery = NAN;
    g_obd.atf = NAN; g_obd.oil_press = NAN; g_obd.gear = -1; g_obd.sel_range = -1;
    await_resp_id = 0; await_deadline = 0; itp_active = false; itp_deadline = 0;
    rx_bad_cnt = 0; tx_fail_cnt = 0; mil_miss = 0;
    last_map_kpa = NAN; baro_kpa = NAN; map_upd_ms = 0;
    memset(&oilp, 0, sizeof oilp);                /* back to OILP_VIA22 */
    filter_status = HAL_OK; start_status = HAL_OK;
    notif_status = HAL_ERROR;          /* default: polled fallback, as the old tests assume */
    nvic_on = false;
    rxq_head = rxq_tail = 0; rxq_drop = 0; rxq_hwm = 0; rx_irq = false; rx_ms_cur = 0;
    init_calls = 0; error_calls = 0; start_calls = 0; start_fail_cnt = 0;
    hmock.Init.StdFiltersNbr   = 4;                 /* as MX_FDCAN1_Init sets it */
    hmock.Init.RxFifo0ElmtsNbr = 16;
    hmock.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    hmock.State = HAL_FDCAN_STATE_READY;
    obd_init(&hmock);
}

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

/* ---------------------------------------------------------------- tests */
static void t_sf_rpm(void)
{
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(near(g_obd.rpm, 2000.0f));
    CHECK(rx_bad_cnt == 0);
}

static void t_sf_grouped_exact_fit(void)
{
    PUSH(0x7E8, 8, 0x06, 0x41, 0x0C, 0x1F, 0x40, 0x0D, 0x43, 0x00);   /* rpm + speed 67 */
    obd_rx_poll();
    CHECK(near(g_obd.rpm, 2000.0f));
    CHECK(near(g_obd.speed, 60.0f));                                   /* 67 * 60/67 */
}

static void t_sf_len15_rejected(void)                                  /* R7 */
{
    PUSH(0x7E8, 8, 0x0F, 0x41, 0x0C, 0x1F, 0x40, 0x05, 0x50, 0x0D);
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.rpm));
}

static void t_sf_truncated_pid_rejected(void)                          /* N1 */
{
    PUSH(0x7E8, 8, 0x03, 0x41, 0x0C, 0x1F, 0x00, 0x00, 0x00, 0x00);   /* rpm needs 2 bytes */
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.rpm));                                            /* not 0 rpm */
}

static void t_sf_short_dlc_rejected(void)                              /* N1 */
{
    PUSH(0x7E8, 3, 0x04, 0x41, 0x0C);                                  /* SF_DL 4 > DLC-1 */
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.rpm));
}

static void t_unknown_pid_ignored_quietly(void)
{
    PUSH(0x7E8, 8, 0x03, 0x41, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(rx_bad_cnt == 0);
    CHECK(updates == 0);
}

static void t_mode01_only_from_ecm(void)
{
    PUSH(0x7E9, 8, 0x03, 0x41, 0x05, 0x5A, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(isnan(g_obd.cool));
    PUSH(0x7E8, 8, 0x03, 0x41, 0x05, 0x5A, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(near(g_obd.cool, 50.0f));
}

static void t_mode22_source_checked(void)
{
    PUSH(0x7E8, 8, 0x04, 0x62, 0x19, 0x40, 0x64, 0x00, 0x00, 0x00);   /* ATF from ECM: no */
    obd_rx_poll();
    CHECK(isnan(g_obd.atf));
    PUSH(0x7EA, 8, 0x04, 0x62, 0x19, 0x40, 0x64, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(near(g_obd.atf, 60.0f));
}

static void egt_request(void)
{
    static const uint8_t pid = 0x78;
    req_mode01(&pid, 1);
}

static void t_multiframe_egt_ok(void)
{
    egt_request();
    CHECK(tx_n == 1);
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);   /* FF, 11 bytes */
    obd_rx_poll();
    CHECK(tx_n == 2 && txlog[1].id == 0x7E0 && txlog[1].data[0] == 0x30);   /* our FC */
    CHECK(itp_active);
    PUSH(0x7E8, 8, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(!itp_active);
    CHECK(near(g_obd.egt, 260.0f));                                     /* 3000/10 - 40 */
    CHECK(await_resp_id == 0);
}

static void t_ff_too_long_no_fc(void)                                  /* N5 */
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x41, 0x41, 0x78, 0x00, 0x00, 0x00, 0x00);   /* FF_DL 65 */
    obd_rx_poll();
    CHECK(tx_n == 1);                                                   /* no FC */
    CHECK(!itp_active);
    CHECK(rx_bad_cnt == 1);
}

static void t_cf_empty_rejected(void)                                  /* A2 */
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    PUSH(0x7E8, 1, 0x21);                                               /* PCI only */
    obd_rx_poll();
    CHECK(!itp_active);
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.egt));
}

static void t_cf_short_rejected(void)                                  /* A2 */
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    PUSH(0x7E8, 4, 0x21, 0x00, 0x00, 0x00);                             /* 3 of 5 bytes */
    obd_rx_poll();
    CHECK(!itp_active);
    CHECK(rx_bad_cnt == 1);
}

static void t_cf_wrong_sequence(void)
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    PUSH(0x7E8, 8, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(!itp_active);
    CHECK(rx_bad_cnt == 1);
}

static void t_cf_other_sender_ignored(void)
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    PUSH(0x7EA, 8, 0x21, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11);   /* someone else's */
    obd_rx_poll();
    CHECK(itp_active);
    PUSH(0x7E8, 8, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(near(g_obd.egt, 260.0f));
}

static void t_reassembly_times_out(void)                               /* A2 */
{
    egt_request();
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    obd_rx_poll();
    CHECK(itp_active);
    now_ms += ITP_TIMEOUT_MS - 1u;
    obd_poll_tick();
    CHECK(itp_active);                                  /* not yet            */
    now_ms += 1u;
    obd_poll_tick();
    CHECK(!itp_active);                                 /* 1 s without a CF   */
    CHECK(await_resp_id == 0 || await_key != 0x78);
}

static void t_foreign_sf_keeps_slot(void)                              /* A1 */
{
    static const uint8_t rpm_pid = 0x0C;
    req_mode01(&rpm_pid, 1);
    CHECK(await_resp_id == 0x7E8);
    PUSH(0x7E8, 8, 0x03, 0x41, 0x05, 0x5A, 0x00, 0x00, 0x00, 0x00);   /* other tester's PID */
    obd_rx_poll();
    CHECK(near(g_obd.cool, 50.0f));                                     /* still decoded */
    CHECK(await_resp_id == 0x7E8);                                      /* slot kept */
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(await_resp_id == 0);                                          /* ours closes it */
}

static void t_foreign_ff_gets_no_fc(void)                              /* A1 */
{
    static const uint8_t rpm_pid = 0x0C;
    req_mode01(&rpm_pid, 1);
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);   /* EGT: not ours */
    obd_rx_poll();
    CHECK(tx_n == 1);                                                   /* no FC sent */
    CHECK(!itp_active);
}

static void t_tx_fail_no_pending(void)                                 /* A1 */
{
    static const uint8_t rpm_pid = 0x0C;
    tx_status = HAL_ERROR;
    req_mode01(&rpm_pid, 1);
    CHECK(await_resp_id == 0);
    CHECK(tx_fail_cnt == 1);
}

static void t_nrc78_keeps_slot(void)
{
    req_mode22(OBD_REQ_TCM2, 0x1940);
    CHECK(await_resp_id == 0x7EA);
    PUSH(0x7EA, 8, 0x03, 0x7F, 0x22, 0x78, 0x00, 0x00, 0x00, 0x00);   /* response pending */
    obd_rx_poll();
    CHECK(await_resp_id == 0x7EA);
    PUSH(0x7EA, 8, 0x03, 0x7F, 0x22, 0x31, 0x00, 0x00, 0x00, 0x00);   /* out of range */
    obd_rx_poll();
    CHECK(await_resp_id == 0);
    CHECK(g_obd.last_nrc_sid == 0x22 && g_obd.last_nrc == 0x31);
}

static void t_mil_goes_stale(void)                                     /* A3 */
{
    PUSH(0x7E8, 8, 0x06, 0x41, 0x01, 0x00, 0x07, 0xE5, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.mil_valid && !g_obd.mil && g_obd.dtc_count == 0);
    /* The ECM goes quiet, the selector broadcast keeps can_ok alive. */
    for (int i = 0; i < 400; i++) {
        tx_n = 0;
        now_ms += OBD_AWAIT_MS;                     /* each tick times the last out */
        obd_poll_tick();
        PUSH(0x1F5, 8, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00);
        obd_rx_poll();
    }
    CHECK(g_obd.can_ok);
    CHECK(!g_obd.mil_valid);
}

static void t_mil_stays_fresh_when_answered(void)
{
    for (int i = 0; i < 400; i++) {
        tx_n = 0;
        now_ms += OBD_AWAIT_MS;                     /* each tick times the last out */
        obd_poll_tick();
        if (tx_n == 1 && txlog[0].data[1] == 0x01 && txlog[0].data[2] == 0x01)
            PUSH(0x7E8, 8, 0x06, 0x41, 0x01, 0x00, 0x07, 0xE5, 0x00, 0x00);
        obd_rx_poll();
    }
    CHECK(g_obd.mil_valid);
}

static void t_extended_id_dropped(void)
{
    const uint8_t b[] = { 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00 };
    push_frame(0x7E8, FDCAN_EXTENDED_ID, 8, b, (int)sizeof b);
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.rpm));
}

static void t_selector_dlc_checked(void)
{
    PUSH(0x1F5, 3, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(g_obd.sel_range == -1);
    PUSH(0x1F5, 8, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.sel_range == 4);
}

static void t_dlc15_classic_frame(void)                                /* HAL copies 64 B */
{
    PUSH(0x1F5, 15, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.sel_range == 2);
}

/* --- Oil pressure, PID 0xA22C: $22 first, else $2C/$AA ------------------- */
static bool last_tx_is(uint32_t id, int n, const uint8_t *b)
{
    return tx_n > 0 && txlog[tx_n - 1].id == id && memcmp(txlog[tx_n - 1].data, b, (size_t)n) == 0;
}

/* Selector in P, fresh: standstill for the $2C gate. */
static void park(void) { g_obd.sel_range = 1; g_obd.sel_upd_ms = now_ms; }

static void t_oilp_via22(void)
{
    CHECK(req_oilp());
    static const uint8_t q[] = { 0x03, 0x22, 0xA2, 0x2C };
    CHECK(last_tx_is(OBD_REQ_ECM, 4, q));
    PUSH(0x7E8, 8, 0x04, 0x62, 0xA2, 0x2C, 0x4B, 0x00, 0x00, 0x00);  /* 75 x 4 kPa */
    obd_rx_poll();
    CHECK(near(g_obd.oil_press, 3.0f));
    CHECK(g_obd.upd_ms[M_OILP] == now_ms);
    CHECK(await_resp_id == 0);
    CHECK(oilp.mode == OILP_VIA22);
    CHECK(obd_is_fresh(&g_obd, M_OILP, now_ms + metric_stale_ms[M_OILP]));
    CHECK(!obd_is_fresh(&g_obd, M_OILP, now_ms + metric_stale_ms[M_OILP] + 1u));
}

#if OILP_DPID_ENABLE
static void t_oilp_22_refused_then_dpid(void)
{
    park();
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x22, 0x31, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_DEFINE && oilp.nrc22 == 0x31);
    CHECK(await_resp_id == 0);

    CHECK(req_oilp());
    static const uint8_t def[] = { 0x04, 0x2C, 0xFE, 0xA2, 0x2C };
    CHECK(last_tx_is(OBD_REQ_ECM, 5, def));
    PUSH(0x7E8, 8, 0x02, 0x6C, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_READ);
    CHECK(await_resp_id == 0);

    CHECK(req_oilp());
    static const uint8_t rd[] = { 0x03, 0xAA, 0x01, 0xFE };
    CHECK(last_tx_is(OBD_REQ_ECM, 4, rd));
    PUSH(0x5E8, 8, 0xFE, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);  /* UUDT, no PCI */
    obd_rx_poll();
    CHECK(near(g_obd.oil_press, 3.0f));
    CHECK(await_resp_id == 0);
    CHECK(oilp.mode == OILP_READ);                      /* keeps reading */
    CHECK(rx_bad_cnt == 0);
}

/* A UUDT packet nobody of ours asked for — another tester's logging — is not
 * decoded, and one with a different DPID does not close our read. */
static void t_oilp_foreign_uudt_ignored(void)
{
    PUSH(0x5E8, 8, 0xFE, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(isnan(g_obd.oil_press));
    oilp.mode = OILP_READ;
    CHECK(req_oilp());
    PUSH(0x5E8, 8, 0xFD, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(isnan(g_obd.oil_press));
    CHECK(await_resp_id == 0x7E8 && await_sid == 0xAA);  /* still waiting */
    PUSH(0x5E8, 1, 0xFE);                                /* truncated */
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.oil_press));
}

/* Another tester's $2C answer must not move our path along. */
static void t_oilp_foreign_define_ignored(void)
{
    oilp.mode = OILP_DEFINE;
    PUSH(0x7E8, 8, 0x02, 0x6C, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_DEFINE);
}

static void t_oilp_2c_refused_ends_search(void)
{
    park();
    oilp.mode = OILP_DEFINE;
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x2C, 0x31, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_NONE && oilp.nrc2c == 0x31);
    CHECK(!req_oilp());
    int asked = 0;                                      /* and the slot is reused */
    for (int i = 0; i < 200; i++) {
        tx_n = 0;
        now_ms += OBD_AWAIT_MS;
        obd_poll_tick();
        for (int k = 0; k < tx_n; k++) {
            const uint8_t *q = txlog[k].data;
            if (q[1] == 0x2C || q[1] == 0xAA || (q[1] == 0x22 && q[2] == 0xA2)) asked++;
        }
    }
    CHECK(asked == 0);
}

#endif /* OILP_DPID_ENABLE */

static void t_oilp_busy_nrc_retried(void)
{
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00);  /* busy, repeat */
    obd_rx_poll();
    CHECK(oilp.mode == OILP_VIA22);
    CHECK(await_resp_id == 0);
}

static void t_oilp_22_silent_falls_back(void)
{
    for (unsigned i = 0; i < OILP_MISS22_MAX; i++) {
        CHECK(oilp.mode == OILP_VIA22);
        CHECK(req_oilp());
        now_ms += OBD_AWAIT_MS;
        obd_poll_tick();                                /* times out (and sends the next) */
        await_resp_id = 0;
    }
    CHECK(oilp.mode == OILP_AFTER_22);
}

/* 0xFF is the top of the byte (10.2 bar): a no-data code, never a reading. */
static void t_oilp_ff_is_not_a_reading(void)
{
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x04, 0x62, 0xA2, 0x2C, 0xFF, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(isnan(g_obd.oil_press));
    CHECK(oilp.have_raw && oilp.raw == 0xFF);           /* still visible on DIAG */
    CHECK(await_resp_id == 0);                          /* answered all the same */
}

#if OILP_DPID_ENABLE
/* $2C only at standstill: fresh P/N, or a fresh 0 km/h; unknown is not. */
static void t_oilp_define_waits_for_standstill(void)
{
    oilp.mode = OILP_DEFINE;
    CHECK(!req_oilp() && tx_n == 0);                    /* nothing known       */
    g_obd.sel_range = 4; g_obd.sel_upd_ms = now_ms;     /* D                   */
    CHECK(!req_oilp() && tx_n == 0);
    g_obd.sel_range = 1;                                /* P, but rolling      */
    g_obd.speed = 30.0f; g_obd.upd_ms[M_SPEED] = now_ms;
    CHECK(!req_oilp() && tx_n == 0);
    g_obd.speed = 0.0f;                                 /* P, stopped          */
    CHECK(req_oilp() && tx_n == 1 && txlog[0].data[1] == 0x2C);
    await_resp_id = 0;
    now_ms += SEL_STALE_MS + 1u;                        /* selector gone stale */
    g_obd.upd_ms[M_SPEED] = now_ms;                     /* speed 0, fresh      */
    CHECK(req_oilp() && tx_n == 2);
    /* and the slot is not wasted while it waits: a fast request goes instead */
    await_resp_id = 0; tx_n = 0;
    g_obd.speed = 50.0f;
    sched.tick = 5u; sched.med_idx = 6u;                /* next tick: medium item 6 */
    obd_poll_tick();
    CHECK(tx_n == 1 && txlog[0].data[1] == 0x01);
}

/* At most OILP_DEFINE_MAX $2C per power-up, then the search ends. */
static void t_oilp_define_capped(void)
{
    park();
    oilp.mode = OILP_DEFINE;
    for (unsigned i = 0; i < OILP_DEFINE_MAX; i++) {
        park();
        CHECK(req_oilp());
        now_ms += OBD_AWAIT_MS;                         /* no answer */
        txn_failed();
    }
    int sent = tx_n;
    park();
    CHECK(!req_oilp());
    CHECK(tx_n == sent && oilp.mode == OILP_NONE);
}

/* No packet after $AA (or an NRC for it): the ECM may have lost the
 * definition, so the next step defines it again. */
static void t_oilp_lost_read_redefines(void)
{
    oilp.mode = OILP_READ;
    CHECK(req_oilp());
    now_ms += OBD_AWAIT_MS;
    obd_poll_tick();
    CHECK(oilp.mode == OILP_DEFINE);
    await_resp_id = 0;
    oilp.mode = OILP_READ;
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x03, 0x7F, 0xAA, 0x31, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_DEFINE && oilp.nrcaa == 0x31);
}
#else
/* OILP_DPID_ENABLE 0: a refused $22 ends the search; nothing else is ever
 * sent for oil pressure (the TX mock fails any $2C / $AA). */
static void t_oilp_off_never_defines(void)
{
    park();
    CHECK(req_oilp());
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x22, 0x31, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(oilp.mode == OILP_NONE);
    for (int i = 0; i < 500; i++) { park(); now_ms += OBD_AWAIT_MS; obd_poll_tick(); }
    oilp.mode = OILP_DEFINE;                            /* even if forced */
    CHECK(!req_oilp());
    oilp.mode = OILP_READ;
    CHECK(!req_oilp());
}
#endif

/* --- Cases from the independent stage-A review (Dash_Stage_A_Adversarial_Tests.c),
 * which were red before the one-transaction-at-a-time rework: a follow-up
 * request must not replace, or be cleared by, a transaction still in flight. */
static void t_interleaved_cf_clears_new_pending(void)
{
    static const uint8_t egt = 0x78;
    req_mode01(&egt, 1);
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    obd_rx_poll();
    CHECK(itp_active);
    int tx_before = tx_n;
    obd_poll_tick();  /* Should not issue another request to this busy ECM. */
    int extra_ecm = 0;
    for (int i = tx_before; i < tx_n; i++)
        if (txlog[i].id == OBD_REQ_ECM && txlog[i].data[0] != 0x30) extra_ecm++;
    CHECK(extra_ecm == 0);
    bool replaced = (await_key != 0x78);
    uint32_t new_id = await_resp_id;
    uint16_t new_key = await_key;
    PUSH(0x7E8, 8, 0x21, 0, 0, 0, 0, 0, 0, 0);
    obd_rx_poll();
    CHECK(near(g_obd.egt, 260.f));
    /* Old CF must never close a different pending transaction. */
    if (replaced) CHECK(await_resp_id == new_id && await_key == new_key);
    else CHECK(await_resp_id == 0);
}

static void t_matching_short_frame_clears_pending(void)
{
    static const uint8_t rpm = 0x0C;
    req_mode01(&rpm, 1);
    CHECK(await_resp_id == 0x7E8);
    PUSH(0x7E8, 8, 0x02, 0x41, 0x0C, 0, 0, 0, 0, 0);
    obd_rx_poll();
    CHECK(rx_bad_cnt == 1);
    CHECK(isnan(g_obd.rpm));
    /* expected: invalid SF must not satisfy an outstanding RPM request */
    CHECK(await_resp_id == 0x7E8);
}

static void t_pending_replaced_before_timeout(void)
{
    req_mode22(OBD_REQ_TCM2, 0x1940);
    CHECK(await_resp_id == OBD_RESP_TCM2 && await_key == 0x1940);
    req_mode22(OBD_REQ_TCM2, 0x199A);
    /* expected: either skip second request or retain first until resolved */
    CHECK(await_resp_id == OBD_RESP_TCM2 && await_key == 0x1940);
}

static void t_response_pending_loses_delayed_ff(void)
{
    static const uint8_t egt = 0x78;
    req_mode01(&egt, 1);
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x01, 0x78, 0, 0, 0, 0);
    obd_rx_poll();
    CHECK(await_resp_id == 0x7E8);
    obd_poll_tick();  /* sends a fresh RPM/MAP request while ECU still busy */
    int tx_before_ff = tx_n;
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0);
    obd_rx_poll();
    /* expected: FF for pending request triggers FC despite intervening tick */
    CHECK(tx_n == tx_before_ff + 1);
    CHECK(itp_active);
}

/* --- Transaction lifecycle ------------------------------------------------ */
static void t_scheduler_waits_then_resumes(void)
{
    obd_poll_tick();                                    /* first request out */
    CHECK(tx_n == 1 && await_resp_id != 0);
    uint32_t tick0 = sched.tick;
    now_ms += OBD_AWAIT_MS - 1u;
    obd_poll_tick();                                    /* unanswered: waits */
    CHECK(tx_n == 1);
    CHECK(sched.tick == tick0);                         /* band not advanced */
    now_ms += 1u;
    obd_poll_tick();                                    /* times out, next one goes */
    CHECK(tx_n == 2);
    CHECK(sched.tick == tick0 + 1);
}

/* A stalled loop must not time out a request whose answer arrived in time:
 * the reply is processed (obd_rx_poll) before the deadline is checked. */
static void t_stall_does_not_fake_timeout(void)
{
    static const uint8_t rpm = 0x0C;
    req_mode01(&rpm, 1);
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);  /* arrived at once */
    now_ms += 2000u;                                    /* loop stalled 2 s     */
    obd_rx_poll();                                      /* cluster_app_run order */
    obd_poll_tick();
    CHECK(near(g_obd.rpm, 2000.0f));
    CHECK(tx_n == 2);                                   /* answered, next sent  */
    CHECK(mil_miss == 0);
}

static void t_nrc78_long_wait_is_bounded(void)
{
    static const uint8_t rpm = 0x0C;
    req_mode01(&rpm, 1);
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x01, 0x78, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    now_ms += OBD_PENDING_MS - 1u;
    obd_poll_tick();
    CHECK(await_resp_id == 0x7E8);                      /* still waiting      */
    CHECK(tx_n == 1);                                   /* nothing else sent  */
    now_ms += 1u;
    obd_poll_tick();
    CHECK(tx_n == 2);                                   /* gave up, moved on  */
}

static void t_fc_tx_fail_aborts_transfer(void)          /* F8 */
{
    egt_request();
    tx_status = HAL_ERROR;                              /* FC will be refused */
    PUSH(0x7E8, 8, 0x10, 0x0B, 0x41, 0x78, 0x01, 0x0B, 0xB8, 0x00);
    obd_rx_poll();
    CHECK(!itp_active);
    CHECK(await_resp_id == 0);
    CHECK(tx_fail_cnt == 1);
}

static void t_mil_invalid_after_three_misses(void)      /* F7 */
{
    static const uint8_t mil = 0x01;
    PUSH(0x7E8, 8, 0x06, 0x41, 0x01, 0x00, 0x07, 0xE5, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.mil_valid);
    for (int miss = 1; miss <= 3; miss++) {
        /* The tick that times a request out sends the next scheduled one;
         * treat that one as answered so the MIL request can go out. */
        await_resp_id = 0;
        CHECK(req_mode01(&mil, 1));
        now_ms += OBD_AWAIT_MS;
        obd_poll_tick();                                /* time out */
        CHECK(await_resp_id == 0 || await_key != 0x01);
        CHECK(g_obd.mil_valid == (miss < 3));           /* invalid on the 3rd */
    }
}

static void t_mil_tx_refusal_counts_as_miss(void)        /* F7 */
{
    static const uint8_t mil = 0x01;
    PUSH(0x7E8, 8, 0x06, 0x41, 0x01, 0x00, 0x07, 0xE5, 0x00, 0x00);
    obd_rx_poll();
    tx_status = HAL_ERROR;
    for (int i = 0; i < 3; i++) req_mode01(&mil, 1);
    CHECK(!g_obd.mil_valid);
}

/* R9: over a long run nothing this E98 never answers (PID 0x5C, PID 0x78,
 * mode 03) is requested, and every signal we do use still is. Requests are
 * left unanswered, so each one also exercises the timeout path. */
static void t_schedule_contents(void)
{
    bool seen_rpm = false, seen_speed = false, seen_map = false, seen_mil = false;
    bool seen_cool = false, seen_rail = false, seen_load = false;
    bool seen_atf = false, seen_gear = false, seen_oil = false, seen_oilp = false;
    int  bad_req = 0;
    for (int i = 0; i < 2000; i++) {
        tx_n = 0;
        now_ms += OBD_AWAIT_MS;
        obd_poll_tick();
        for (int k = 0; k < tx_n; k++) {
            const uint8_t *q = txlog[k].data;
            if (q[1] == 0x03) bad_req++;
            if (q[1] == 0x01) {
                for (int b = 2; b < 1 + q[0]; b++) {
                    if (q[b] == 0x5C || q[b] == 0x78) bad_req++;
                    if (q[b] == 0x0C) seen_rpm = true;
                    if (q[b] == 0x0D) seen_speed = true;
                    if (q[b] == 0x0B) seen_map = true;
                    if (q[b] == 0x01) seen_mil = true;
                    if (q[b] == 0x05) seen_cool = true;
                    if (q[b] == 0x23) seen_rail = true;
                    if (q[b] == 0x04) seen_load = true;
                }
            }
            if (q[1] == 0x22) {
                uint16_t did = (uint16_t)((q[2] << 8) | q[3]);
                if (did == 0x1940 && txlog[k].id == OBD_REQ_TCM2) seen_atf = true;
                if (did == 0x199A && txlog[k].id == OBD_REQ_TCM2) seen_gear = true;
                if (did == 0x1154 && txlog[k].id == OBD_REQ_ECM)  seen_oil = true;
                if (did == 0xA22C && txlog[k].id == OBD_REQ_ECM)  seen_oilp = true;
            }
        }
    }
    CHECK(bad_req == 0);
    CHECK(seen_rpm && seen_speed && seen_map && seen_mil && seen_cool);
    CHECK(seen_rail && seen_load && seen_atf && seen_gear && seen_oil && seen_oilp);
}

/* R1: a CubeMX regen that drops the RX FIFO / filter counts must not leave
 * the controller deaf — obd_init re-lays out the message RAM. */
static void t_layout_reasserted(void)
{
    hmock.Init.StdFiltersNbr   = 0;                 /* what Dash.ioc used to give */
    hmock.Init.RxFifo0ElmtsNbr = 0;
    hmock.State = HAL_FDCAN_STATE_READY;
    init_calls = 0;
    obd_init(&hmock);
    CHECK(init_calls == 1);
    CHECK(hmock.Init.StdFiltersNbr == 3 && hmock.Init.RxFifo0ElmtsNbr == 16);
    CHECK(hmock.State == HAL_FDCAN_STATE_BUSY);     /* started */
    CHECK(error_calls == 0);
}

static void t_layout_ok_no_reinit(void)
{
    CHECK(init_calls == 0);                         /* reset() ran obd_init */
    CHECK(hmock.State == HAL_FDCAN_STATE_BUSY);
}

static void t_filter_failure_is_fatal(void)          /* N2 */
{
    hmock.State = HAL_FDCAN_STATE_READY;
    filter_status = HAL_ERROR;
    start_calls = 0;
    obd_init(&hmock);
    CHECK(error_calls == 1);
    CHECK(start_calls == 0);                        /* never started half-configured */
}

static void t_start_retried_after_failure(void)      /* N2 */
{
    hmock.State = HAL_FDCAN_STATE_READY;            /* e.g. after a bus-off Stop */
    start_status = HAL_ERROR;
    now_ms += 200;
    obd_rx_poll();                                  /* runs the health check */
    CHECK(start_fail_cnt == 1);
    CHECK(hmock.State == HAL_FDCAN_STATE_READY);
    start_status = HAL_OK;
    now_ms += 200;
    obd_rx_poll();
    CHECK(hmock.State == HAL_FDCAN_STATE_BUSY);
}

/* --- RX by interrupt into the ring (stage C) ------------------------------ */
static void irq_mode(void)
{
    notif_status = HAL_OK;
    hmock.State = HAL_FDCAN_STATE_READY;
    obd_init(&hmock);
}

static void t_irq_rx_path(void)
{
    CHECK(!rx_irq);                                 /* reset(): polled fallback */
    irq_mode();
    CHECK(rx_irq && nvic_on);
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();                                  /* loop must not touch the FIFO */
    CHECK(isnan(g_obd.rpm));
    CHECK(hw_head == 0 && hw_tail == 1);
    FDCAN1_IT0_IRQHandler();                        /* the interrupt fires */
    CHECK(hw_head == hw_tail);                      /* FIFO emptied into the ring */
    obd_rx_poll();
    CHECK(near(g_obd.rpm, 2000.0f));
    obd_health_t h;
    obd_can_health(&h);
    CHECK(h.rx_irq);
}

static void t_ring_overflow_counted(void)
{
    irq_mode();
    for (unsigned b = 0; b < RXQ_LEN / 64u; b++) {  /* mock FIFO holds 64 */
        for (int i = 0; i < 64; i++) PUSH(0x1F5, 8, 0, 0, 0, 0x04, 0, 0, 0, 0);
        FDCAN1_IT0_IRQHandler();
    }                                               /* ring now full */
    for (int i = 0; i < 6; i++) PUSH(0x1F5, 8, 0, 0, 0, 0x02, 0, 0, 0, 0);
    FDCAN1_IT0_IRQHandler();                        /* 6 more: no room */
    obd_health_t h;
    obd_can_health(&h);
    CHECK(h.rx_lost == 6);
    CHECK(h.rx_hwm == RXQ_LEN);
    obd_rx_poll();                                  /* drains what was kept */
    CHECK(g_obd.sel_range == 4);
    CHECK(rxq_head == rxq_tail);
}

/* Audit A02: freshness is the time a frame was RECEIVED (ISR), not decoded. A
 * reply that waited 2.4 s in the ring behind a stall is 2.4 s old. */
static void t_delayed_frame_keeps_rx_time(void)
{
    irq_mode();
    uint32_t t0 = now_ms;
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);   /* RPM 2000  */
    PUSH(0x1F5, 8, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00);   /* selector D */
    FDCAN1_IT0_IRQHandler();                        /* received at t0           */
    now_ms += 2400u;                                /* loop stalled 2.4 s       */
    obd_rx_poll();
    CHECK(near(g_obd.rpm, 2000.0f));                /* decoded ...              */
    CHECK(g_obd.upd_ms[M_RPM] == t0);               /* ... stamped when received */
    CHECK(!obd_is_fresh(&g_obd, M_RPM, now_ms));    /* 2400 > 2000: stale       */
    CHECK(g_obd.sel_upd_ms == t0 && !obd_sel_fresh(&g_obd, now_ms));

    uint32_t t1 = now_ms;                           /* a short wait stays fresh */
    PUSH(0x7E8, 8, 0x03, 0x41, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00);   /* coolant 90 */
    FDCAN1_IT0_IRQHandler();
    now_ms += 150u;
    obd_rx_poll();
    CHECK(g_obd.upd_ms[M_COOL] == t1 && obd_is_fresh(&g_obd, M_COOL, now_ms));
    now_ms = t1 + metric_stale_ms[M_COOL] + 1u;
    CHECK(!obd_is_fresh(&g_obd, M_COOL, now_ms));
}

static void t_delayed_frame_across_wrap(void)
{
    irq_mode();
    now_ms = 0xFFFFFF00u;
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    FDCAN1_IT0_IRQHandler();
    now_ms += 0x300u;                               /* 768 ms later, past the wrap */
    obd_rx_poll();
    CHECK(g_obd.upd_ms[M_RPM] == 0xFFFFFF00u);
    CHECK(obd_is_fresh(&g_obd, M_RPM, now_ms));
    now_ms = 0xFFFFFF00u + metric_stale_ms[M_RPM] + 1u;
    CHECK(!obd_is_fresh(&g_obd, M_RPM, now_ms));
}

/* SC-10: the deepest the ring has been is kept for DIAG. */
static void t_ring_high_water_mark(void)
{
    irq_mode();
    for (int i = 0; i < 10; i++) PUSH(0x1F5, 8, 0, 0, 0, 0x01, 0, 0, 0, 0);
    FDCAN1_IT0_IRQHandler();
    obd_health_t h;
    obd_can_health(&h);
    CHECK(h.rx_hwm == 10);
    obd_rx_poll();                                  /* drained */
    for (int i = 0; i < 3; i++) PUSH(0x1F5, 8, 0, 0, 0, 0x01, 0, 0, 0, 0);
    FDCAN1_IT0_IRQHandler();
    obd_can_health(&h);
    CHECK(h.rx_hwm == 10);                          /* a maximum, not a level */
}

static void t_health_check_masks_rx_irq(void)
{
    irq_mode();
    now_ms += 200;
    obd_rx_poll();                                  /* health check runs */
    CHECK(nvic_on);                                 /* re-enabled afterwards */
}

/* --- Per-metric freshness (R3/F6) ------------------------------------------ */
static void t_decode_stamps_fresh(void)
{
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.upd_ms[M_RPM] == now_ms);
    CHECK(obd_is_fresh(&g_obd, M_RPM, now_ms + metric_stale_ms[M_RPM]));
    CHECK(!obd_is_fresh(&g_obd, M_RPM, now_ms + metric_stale_ms[M_RPM] + 1u));
}

static void t_unchanged_value_restamps(void)
{
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    now_ms += 1500;
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);   /* same 2000 rpm */
    obd_rx_poll();
    CHECK(g_obd.upd_ms[M_RPM] == now_ms);           /* a steady value is still fresh */
}

/* R3: the ECM stops answering, the selector broadcast keeps the link "live";
 * the old RPM must go stale instead of staying on screen. */
static void t_stale_while_broadcast_alive(void)
{
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    for (int i = 0; i < 30; i++) {                  /* 3 s of broadcasts only */
        now_ms += 100;
        PUSH(0x1F5, 8, 0, 0, 0, 0x04, 0, 0, 0, 0);
        obd_rx_poll();
    }
    CHECK(g_obd.can_ok);
    CHECK(near(g_obd.rpm, 2000.0f));                /* last value kept ...  */
    CHECK(!obd_is_fresh(&g_obd, M_RPM, now_ms));    /* ... but not shown    */
    CHECK(obd_sel_fresh(&g_obd, now_ms));           /* selector itself fresh */
}

static void t_selector_freshness(void)
{
    PUSH(0x1F5, 8, 0, 0, 0, 0x01, 0, 0, 0, 0);
    obd_rx_poll();
    CHECK(obd_sel_fresh(&g_obd, now_ms + SEL_STALE_MS));
    CHECK(!obd_sel_fresh(&g_obd, now_ms + SEL_STALE_MS + 1u));
}

static void t_gear_and_boost_stamped(void)
{
    PUSH(0x7EA, 8, 0x04, 0x62, 0x19, 0x9A, 0x03, 0x00, 0x00, 0x00);   /* gear 3 */
    PUSH(0x7E8, 8, 0x03, 0x41, 0x33, 0x65, 0x00, 0x00, 0x00, 0x00);   /* baro 101 kPa */
    PUSH(0x7E8, 8, 0x03, 0x41, 0x0B, 0x96, 0x00, 0x00, 0x00, 0x00);   /* MAP 150 kPa */
    obd_rx_poll();
    CHECK(g_obd.gear == 3 && g_obd.upd_ms[M_GEAR] == now_ms);
    CHECK(near(g_obd.boost, 0.49f) && g_obd.upd_ms[M_BOOST] == now_ms);
}

/* C01: boost needs a real baro, and its freshness is MAP's. */
static void t_boost_needs_baro_and_follows_map(void)
{
    PUSH(0x7E8, 8, 0x03, 0x41, 0x0B, 0x96, 0x00, 0x00, 0x00, 0x00);   /* MAP, no baro yet */
    obd_rx_poll();
    CHECK(isnan(g_obd.boost));                      /* no assumed 101 kPa */
    PUSH(0x7E8, 8, 0x03, 0x41, 0x33, 0x64, 0x00, 0x00, 0x00, 0x00);   /* baro 100 kPa */
    obd_rx_poll();
    CHECK(near(g_obd.boost, 0.50f));
    uint32_t t_map = now_ms;
    now_ms += 3000u;                                /* MAP goes quiet ... */
    PUSH(0x7E8, 8, 0x03, 0x41, 0x33, 0x64, 0x00, 0x00, 0x00, 0x00);   /* ... baro still answers */
    obd_rx_poll();
    CHECK(g_obd.upd_ms[M_BOOST] == t_map);          /* baro did not refresh it */
    CHECK(!obd_is_fresh(&g_obd, M_BOOST, now_ms));
}

/* HAL tick wrap (~49.7 days): freshness and deadlines use unsigned / signed
 * differences, so they survive the wrap. */
static void t_tick_wraparound(void)
{
    now_ms = 0xFFFFFF00u;
    PUSH(0x7E8, 8, 0x04, 0x41, 0x0C, 0x1F, 0x40, 0x00, 0x00, 0x00);
    obd_rx_poll();
    now_ms += 0x200u;                               /* wrapped past 0 */
    CHECK(obd_is_fresh(&g_obd, M_RPM, now_ms));     /* 512 ms old: fresh */
    static const uint8_t rpm = 0x0C;
    now_ms = 0xFFFFFFF0u;
    await_resp_id = 0;
    CHECK(req_mode01(&rpm, 1));                     /* deadline wraps */
    int sent = tx_n;
    now_ms += 100u;
    obd_poll_tick();
    CHECK(tx_n == sent);                            /* still waiting */
    now_ms += OBD_AWAIT_MS;
    obd_poll_tick();
    CHECK(tx_n == sent + 1);                        /* timed out across the wrap, next sent */
}

static void t_sniff_pauses_polling(void)
{
    sniff_on = true;
    for (int i = 0; i < 10; i++) obd_poll_tick();
    CHECK(tx_n == 0);
    sniff_on = false;
    obd_poll_tick();
    CHECK(tx_n == 1);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "sf_rpm",                     t_sf_rpm },
        { "sf_grouped_exact_fit",       t_sf_grouped_exact_fit },
        { "sf_len15_rejected",          t_sf_len15_rejected },
        { "sf_truncated_pid_rejected",  t_sf_truncated_pid_rejected },
        { "sf_short_dlc_rejected",      t_sf_short_dlc_rejected },
        { "unknown_pid_ignored_quietly",t_unknown_pid_ignored_quietly },
        { "mode01_only_from_ecm",       t_mode01_only_from_ecm },
        { "mode22_source_checked",      t_mode22_source_checked },
        { "multiframe_egt_ok",          t_multiframe_egt_ok },
        { "ff_too_long_no_fc",          t_ff_too_long_no_fc },
        { "cf_empty_rejected",          t_cf_empty_rejected },
        { "cf_short_rejected",          t_cf_short_rejected },
        { "cf_wrong_sequence",          t_cf_wrong_sequence },
        { "cf_other_sender_ignored",    t_cf_other_sender_ignored },
        { "reassembly_times_out",       t_reassembly_times_out },
        { "foreign_sf_keeps_slot",      t_foreign_sf_keeps_slot },
        { "foreign_ff_gets_no_fc",      t_foreign_ff_gets_no_fc },
        { "tx_fail_no_pending",         t_tx_fail_no_pending },
        { "nrc78_keeps_slot",           t_nrc78_keeps_slot },
        { "mil_goes_stale",             t_mil_goes_stale },
        { "mil_stays_fresh",            t_mil_stays_fresh_when_answered },
        { "extended_id_dropped",        t_extended_id_dropped },
        { "selector_dlc_checked",       t_selector_dlc_checked },
        { "dlc15_classic_frame",        t_dlc15_classic_frame },
        { "oilp_via22",                 t_oilp_via22 },
        { "oilp_busy_nrc_retried",      t_oilp_busy_nrc_retried },
        { "oilp_22_silent_falls_back",  t_oilp_22_silent_falls_back },
        { "oilp_ff_is_not_a_reading",   t_oilp_ff_is_not_a_reading },
#if OILP_DPID_ENABLE
        { "oilp_22_refused_then_dpid",  t_oilp_22_refused_then_dpid },
        { "oilp_foreign_uudt_ignored",  t_oilp_foreign_uudt_ignored },
        { "oilp_foreign_define_ignored",t_oilp_foreign_define_ignored },
        { "oilp_2c_refused_ends_search",t_oilp_2c_refused_ends_search },
        { "oilp_define_waits_standstill",t_oilp_define_waits_for_standstill },
        { "oilp_define_capped",         t_oilp_define_capped },
        { "oilp_lost_read_redefines",   t_oilp_lost_read_redefines },
#else
        { "oilp_off_never_defines",     t_oilp_off_never_defines },
#endif
        { "rev_response_pending_lost_ff", t_response_pending_loses_delayed_ff },
        { "rev_interleaved_cf_pending", t_interleaved_cf_clears_new_pending },
        { "rev_short_sf_keeps_pending", t_matching_short_frame_clears_pending },
        { "rev_pending_not_replaced",   t_pending_replaced_before_timeout },
        { "scheduler_waits_resumes",    t_scheduler_waits_then_resumes },
        { "stall_does_not_fake_timeout",t_stall_does_not_fake_timeout },
        { "nrc78_long_wait_bounded",    t_nrc78_long_wait_is_bounded },
        { "fc_tx_fail_aborts",          t_fc_tx_fail_aborts_transfer },
        { "mil_invalid_after_3_misses", t_mil_invalid_after_three_misses },
        { "mil_tx_refusal_is_miss",     t_mil_tx_refusal_counts_as_miss },
        { "sniff_pauses_polling",       t_sniff_pauses_polling },
        { "schedule_contents",          t_schedule_contents },
        { "layout_reasserted",          t_layout_reasserted },
        { "layout_ok_no_reinit",        t_layout_ok_no_reinit },
        { "filter_failure_is_fatal",    t_filter_failure_is_fatal },
        { "start_retried_after_failure",t_start_retried_after_failure },
        { "irq_rx_path",                t_irq_rx_path },
        { "ring_overflow_counted",      t_ring_overflow_counted },
        { "health_check_masks_rx_irq",  t_health_check_masks_rx_irq },
        { "delayed_frame_keeps_rx_time",t_delayed_frame_keeps_rx_time },
        { "delayed_frame_across_wrap",  t_delayed_frame_across_wrap },
        { "ring_high_water_mark",       t_ring_high_water_mark },
        { "decode_stamps_fresh",        t_decode_stamps_fresh },
        { "unchanged_value_restamps",   t_unchanged_value_restamps },
        { "stale_while_broadcast_alive",t_stale_while_broadcast_alive },
        { "selector_freshness",         t_selector_freshness },
        { "gear_and_boost_stamped",     t_gear_and_boost_stamped },
        { "boost_needs_baro_follows_map",t_boost_needs_baro_and_follows_map },
        { "tick_wraparound",            t_tick_wraparound },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        reset();
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
