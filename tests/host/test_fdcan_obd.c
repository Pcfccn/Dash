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

/* ------------------------------------------------------------- mock HAL */
static uint32_t now_ms;
uint32_t HAL_GetTick(void) { return now_ms; }

typedef struct { uint32_t id, idtype, ftype, dlc; uint8_t data[64]; } rxf_t;
static rxf_t rxq[64];
static int   rx_head, rx_tail;

typedef struct { uint32_t id; uint8_t data[8]; } txf_t;
static txf_t             txlog[64];
static int               tx_n;
static HAL_StatusTypeDef tx_status;

static const uint8_t DLC2B[16] = { 0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64 };

static HAL_StatusTypeDef filter_status, start_status;
static int init_calls, error_calls, start_calls;

void Error_Handler(void) { error_calls++; }      /* firmware: record + hang */

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
    if (tx_status != HAL_OK) return tx_status;
    if (tx_n < 64) {
        txlog[tx_n].id = tx->Identifier;
        memcpy(txlog[tx_n].data, data, 8);
        tx_n++;
    }
    return HAL_OK;
}

uint32_t HAL_FDCAN_GetRxFifoFillLevel(FDCAN_HandleTypeDef *h, uint32_t fifo)
{ (void)h; (void)fifo; return (uint32_t)(rx_tail - rx_head); }

/* Copies DLCtoBytes[DLC] bytes, exactly like the real HAL — up to 64. */
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h, uint32_t loc,
                                         FDCAN_RxHeaderTypeDef *rh, uint8_t *data)
{
    (void)h; (void)loc;
    if (rx_head == rx_tail) return HAL_ERROR;
    const rxf_t *f = &rxq[rx_head++];
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
static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void push_frame(uint32_t id, uint32_t idtype, uint32_t dlc, const uint8_t *b, int n)
{
    if (rx_head == rx_tail) rx_head = rx_tail = 0;   /* drained: restart the queue */
    if (rx_tail >= (int)(sizeof rxq / sizeof rxq[0])) { printf("  rx queue overflow\n"); fails++; return; }
    rxf_t *f = &rxq[rx_tail++];
    f->id = id; f->idtype = idtype; f->ftype = FDCAN_DATA_FRAME; f->dlc = dlc;
    memset(f->data, 0xEE, sizeof f->data);           /* poison past the given bytes */
    memcpy(f->data, b, (size_t)n);
}
#define PUSH(id, dlc, ...) do { const uint8_t b_[] = { __VA_ARGS__ }; \
    push_frame((id), FDCAN_STANDARD_ID, (dlc), b_, (int)sizeof b_); } while (0)

static FDCAN_HandleTypeDef hmock;

static void reset(void)
{
    rx_head = rx_tail = 0; tx_n = 0; tx_status = HAL_OK; updates = 0; now_ms = 1000;
    sniff_on = false;
    memset(&sched, 0, sizeof sched);              /* scheduler position */
    await_sid = 0; await_key = 0;
    memset((void *)&g_obd, 0, sizeof g_obd);
    g_obd.speed = NAN; g_obd.rpm = NAN; g_obd.cool = NAN; g_obd.oil = NAN; g_obd.iat = NAN;
    g_obd.load = NAN; g_obd.boost = NAN; g_obd.rail = NAN; g_obd.egt = NAN; g_obd.battery = NAN;
    g_obd.atf = NAN; g_obd.oil_press = NAN; g_obd.gear = -1; g_obd.sel_range = -1;
    await_resp_id = 0; await_ttl = 0; itp_active = false; itp_ttl = 0;
    rx_bad_cnt = 0; tx_fail_cnt = 0; mil_miss = 0;
    last_map_kpa = NAN; baro_kpa = 101.0f;
    filter_status = HAL_OK; start_status = HAL_OK;
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
    for (unsigned i = 0; i < ITP_TTL_TICKS; i++) obd_poll_tick();
    CHECK(!itp_active);
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
        now_ms += 25;
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
        now_ms += 25;
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

static void t_oilp_candidate_not_decoded(void)                         /* R4 */
{
    PUSH(0x0C9, 8, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    CHECK(g_obd.oilp_0c9_raw == 0x90);
    CHECK(isnan(g_obd.oil_press));
}

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
    obd_poll_tick();                                    /* unanswered: waits */
    CHECK(tx_n == 1);
    CHECK(sched.tick == tick0);                         /* band not advanced */
    obd_poll_tick();                                    /* times out, next one goes */
    CHECK(tx_n == 2);
    CHECK(sched.tick == tick0 + 1);
}

static void t_nrc78_long_wait_is_bounded(void)
{
    static const uint8_t rpm = 0x0C;
    req_mode01(&rpm, 1);
    PUSH(0x7E8, 8, 0x03, 0x7F, 0x01, 0x78, 0x00, 0x00, 0x00, 0x00);
    obd_rx_poll();
    for (unsigned i = 0; i < OBD_PENDING_TICKS - 1u; i++) obd_poll_tick();
    CHECK(await_resp_id == 0x7E8);                      /* still waiting      */
    CHECK(tx_n == 1);                                   /* nothing else sent  */
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
        for (unsigned i = 0; i < OBD_AWAIT_TICKS; i++) obd_poll_tick();   /* time out */
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
    bool seen_atf = false, seen_gear = false, seen_oil = false;
    int  bad_req = 0;
    for (int i = 0; i < 2000; i++) {
        tx_n = 0;
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
            }
        }
    }
    CHECK(bad_req == 0);
    CHECK(seen_rpm && seen_speed && seen_map && seen_mil && seen_cool);
    CHECK(seen_rail && seen_load && seen_atf && seen_gear && seen_oil);
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
    CHECK(hmock.Init.StdFiltersNbr == 4 && hmock.Init.RxFifo0ElmtsNbr == 16);
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
        { "oilp_candidate_not_decoded", t_oilp_candidate_not_decoded },
        { "rev_response_pending_lost_ff", t_response_pending_loses_delayed_ff },
        { "rev_interleaved_cf_pending", t_interleaved_cf_clears_new_pending },
        { "rev_short_sf_keeps_pending", t_matching_short_frame_clears_pending },
        { "rev_pending_not_replaced",   t_pending_replaced_before_timeout },
        { "scheduler_waits_resumes",    t_scheduler_waits_then_resumes },
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
