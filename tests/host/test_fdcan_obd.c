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

HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *h, FDCAN_FilterTypeDef *f)
{ (void)h; (void)f; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *h, uint32_t a, uint32_t b,
                                               uint32_t c, uint32_t d)
{ (void)h; (void)a; (void)b; (void)c; (void)d; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef *h)  { (void)h; return HAL_OK; }

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
static int updates;
void obd_on_update(void) { updates++; }
void can_sniff_feed(uint16_t id, const uint8_t *d, uint8_t len) { (void)id; (void)d; (void)len; }
bool can_sniff_is_active(void) { return false; }

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
    memset((void *)&g_obd, 0, sizeof g_obd);
    g_obd.speed = NAN; g_obd.rpm = NAN; g_obd.cool = NAN; g_obd.oil = NAN; g_obd.iat = NAN;
    g_obd.load = NAN; g_obd.boost = NAN; g_obd.rail = NAN; g_obd.egt = NAN; g_obd.battery = NAN;
    g_obd.atf = NAN; g_obd.oil_press = NAN; g_obd.gear = -1; g_obd.sel_range = -1;
    await_resp_id = 0; await_ttl = 0; itp_active = false; itp_ttl = 0;
    rx_bad_cnt = 0; tx_fail_cnt = 0; mil_miss = 0;
    last_map_kpa = NAN; baro_kpa = 101.0f;
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
