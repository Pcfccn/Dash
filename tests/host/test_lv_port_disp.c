/* Host-side tests for Core/Src/lv_port_disp.c: the flush path's error
 * accounting and the repaint / panel re-init recovery, by fault injection
 * into a mocked SPI/DMA HAL and LVGL (tests/host/mock_lv_port_disp/).
 * This checks the driver's decisions, not the ILI9488, the wiring or the
 * real DMA — those need the bench. */
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/lv_port_disp.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ------------------------------------------------------------- mock HAL */
GPIO_TypeDef      mock_gpiob, mock_gpioc;
SPI_HandleTypeDef hspi2;

static uint32_t now_ms;
static int      tick_autoadvance;      /* HAL_GetTick() advances 1 ms per call */
static int      dc_high;               /* LCD D/C line: 1 = data, 0 = command   */
static int      swreset_cnt;           /* 0x01 commands sent = panel inits      */
static int      dma_started, aborts, error_calls;
static HAL_StatusTypeDef dma_status;   /* result of HAL_SPI_Transmit_DMA        */
static int      fail_all_tx;           /* every blocking transmit fails         */
static int      fail_bulk_tx;          /* blocking transmits > 64 B fail        */
static int      fail_cmd;              /* blocking transmit of this command fails (-1 none) */

uint32_t HAL_GetTick(void) { return tick_autoadvance ? now_ms++ : now_ms; }
void     HAL_Delay(uint32_t ms) { now_ms += ms; }
uint32_t osKernelGetTickCount(void) { return now_ms; }
void     SCB_CleanDCache_by_Addr(uint32_t *a, int32_t n) { (void)a; (void)n; }
void     Error_Handler(void) { error_calls++; }

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState s)
{
    if (port == GPIOC && pin == GPIO_PIN_4) dc_high = (s == GPIO_PIN_SET);
}
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) { (void)port; (void)init; }
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *h) { (void)h; return HAL_OK; }
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *h) { (void)h; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h) { (void)h; return HAL_OK; }
void HAL_SPI_IRQHandler(SPI_HandleTypeDef *h) { (void)h; }
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t p, uint32_t s) { (void)irq; (void)p; (void)s; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { (void)irq; }

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n, uint32_t t)
{
    (void)h; (void)t;
    if (!dc_high && n == 1u) {
        if (p[0] == 0x01u) swreset_cnt++;
        if (fail_cmd >= 0 && p[0] == (uint8_t)fail_cmd) return HAL_TIMEOUT;
    }
    if (fail_all_tx) return HAL_TIMEOUT;
    if (fail_bulk_tx && n > 64u) return HAL_TIMEOUT;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n)
{
    (void)h; (void)p; (void)n;
    if (dma_status == HAL_OK) dma_started++;
    return dma_status;
}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h) { (void)h; aborts++; return HAL_OK; }

/* ------------------------------------------------------------ mock LVGL */
struct lv_display_t { lv_display_flush_cb_t cb; };
struct lv_obj_t     { int dummy; };
static struct lv_display_t the_disp;
static struct lv_obj_t     the_screen;
static int flush_ready_cnt, invalidate_cnt;

void lv_init(void) { }
void lv_tick_set_cb(lv_tick_get_cb_t cb) { (void)cb; }
lv_display_t *lv_display_create(int32_t w, int32_t h) { (void)w; (void)h; return &the_disp; }
void lv_display_set_color_format(lv_display_t *d, int cf) { (void)d; (void)cf; }
void lv_display_set_flush_cb(lv_display_t *d, lv_display_flush_cb_t cb) { d->cb = cb; }
void lv_display_set_buffers(lv_display_t *d, void *a, void *b, uint32_t n, int m)
{ (void)d; (void)a; (void)b; (void)n; (void)m; }
void lv_display_flush_ready(lv_display_t *d) { (void)d; flush_ready_cnt++; }
lv_obj_t *lv_display_get_screen_active(lv_display_t *d) { (void)d; return &the_screen; }
void lv_obj_invalidate(const lv_obj_t *o) { if (o == &the_screen) invalidate_cnt++; }

/* ------------------------------------------------------------- harness */
static uint8_t px[LCD_H_RES * DISP_BUF_LINES * 2];

static void reset_mocks(void)
{
    now_ms = 1000; tick_autoadvance = 0; dc_high = 0; swreset_cnt = 0;
    dma_started = 0; aborts = 0; error_calls = 0; dma_status = HAL_OK;
    fail_all_tx = 0; fail_bulk_tx = 0; fail_cmd = -1;
    flush_ready_cnt = 0; invalidate_cnt = 0; the_disp.cb = NULL;
}

static void reset(void)
{
    reset_mocks();
    s_spi_err = 0; s_err_streak = 0; s_need_repaint = false; s_recover_cnt = 0;
    s_dma_fallback = 0; s_dma_busy = false; s_disp = NULL;
    memset(&s_svc, 0, sizeof s_svc);
    lv_port_disp_init();
    swreset_cnt = 0;                       /* count inits after boot from here */
    s_need_repaint = false;
}

static void flush(int32_t w, int32_t h)
{
    lv_area_t a = { 0, 0, w - 1, h - 1 };
    the_disp.cb(&the_disp, &a, px);
}
static void dma_done(void) { HAL_SPI_TxCpltCallback(&hspi2); }

/* ---------------------------------------------------------------- tests */
static void t_clean_boot(void)
{
    reset_mocks();
    s_spi_err = 0; s_err_streak = 0; s_need_repaint = false; s_recover_cnt = 0;
    memset(&s_svc, 0, sizeof s_svc);
    lv_port_disp_init();
    CHECK(swreset_cnt == 1 && the_disp.cb != NULL && error_calls == 0);
    CHECK(s_spi_err == 0 && s_err_streak == 0 && !s_need_repaint);
}

static void t_good_area(void)
{
    flush(320, 40);
    CHECK(dma_started == 1 && s_dma_busy && flush_ready_cnt == 1);
    dma_done();
    CHECK(!s_dma_busy && s_spi_err == 0);
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(invalidate_cnt == 0);                    /* nothing to repair */
}

/* DMA refused, blocking fallback delivers: counted as F, not an error. */
static void t_dma_refused_fallback_ok(void)
{
    dma_status = HAL_ERROR;
    flush(320, 40);
    CHECK(s_dma_fallback == 1 && s_spi_err == 0 && !s_need_repaint);
    CHECK(flush_ready_cnt == 1);
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(invalidate_cnt == 0 && swreset_cnt == 0);
}

/* DMA refused and the fallback fails: a lost area -> one full repaint. */
static void t_dma_refused_fallback_fails(void)
{
    dma_status = HAL_ERROR; fail_bulk_tx = 1;
    flush(320, 40);
    CHECK(s_dma_fallback == 1 && s_spi_err == 1 && s_need_repaint);
    CHECK(flush_ready_cnt == 1);                   /* LVGL is never left waiting */
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(invalidate_cnt == 1 && swreset_cnt == 0);   /* repaint, no re-init */
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(invalidate_cnt == 1);                    /* once */
}

/* Three lost areas in a row -> the panel is re-initialised, then repainted. */
static void t_three_lost_reinit(void)
{
    dma_status = HAL_ERROR; fail_bulk_tx = 1;
    for (int i = 0; i < 3; i++) flush(320, 40);
    CHECK(s_spi_err == 3 && s_err_streak == 3);
    fail_bulk_tx = 0; dma_status = HAL_OK;
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(swreset_cnt == 1 && s_recover_cnt == 1 && invalidate_cnt == 1);
    CHECK(s_err_streak == 0 && !s_need_repaint);
}

/* A good area in between resets the streak: no re-init for scattered errors. */
static void t_good_area_resets_streak(void)
{
    dma_status = HAL_ERROR; fail_bulk_tx = 1;
    flush(320, 40); flush(320, 40);
    dma_status = HAL_OK; fail_bulk_tx = 0;
    flush(320, 40); dma_done();
    dma_status = HAL_ERROR; fail_bulk_tx = 1;
    flush(320, 40);
    CHECK(s_spi_err == 3 && s_err_streak == 1);
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(swreset_cnt == 0 && invalidate_cnt == 1);
}

/* A re-init that itself fails is retried after the gap, repainting meanwhile. */
static void t_failed_reinit_retried(void)
{
    dma_status = HAL_ERROR; fail_bulk_tx = 1;
    for (int i = 0; i < 3; i++) flush(320, 40);
    fail_cmd = 0x11;                               /* SLEEP OUT fails in the init */
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(swreset_cnt == 1 && s_recover_cnt == 1);
    CHECK(s_err_streak >= LCD_RECOVER_STREAK && s_need_repaint);   /* not done */
    now_ms += LCD_REPAINT_GAP_MS;
    lv_port_disp_service();
    CHECK(swreset_cnt == 1);                       /* inside the 5 s gap: repaint only */
    CHECK(invalidate_cnt == 2);
    fail_cmd = -1; fail_bulk_tx = 0; dma_status = HAL_OK;
    now_ms += LCD_RECOVER_GAP_MS;
    lv_port_disp_service();
    CHECK(swreset_cnt == 2 && s_recover_cnt == 2);
    CHECK(s_err_streak == 0 && !s_need_repaint);
}

/* Boot init with SPI errors: the first service call re-inits the panel. */
static void t_boot_errors_force_reinit(void)
{
    reset_mocks();
    s_spi_err = 0; s_err_streak = 0; s_need_repaint = false; s_recover_cnt = 0;
    memset(&s_svc, 0, sizeof s_svc);
    fail_cmd = 0x29;                               /* DISPLAY ON fails */
    lv_port_disp_init();
    CHECK(s_spi_err == 1 && s_err_streak == LCD_RECOVER_STREAK && s_need_repaint);
    fail_cmd = -1;
    swreset_cnt = 0;
    now_ms += 2000u;
    lv_port_disp_service();
    CHECK(swreset_cnt == 1 && s_recover_cnt == 1 && !s_need_repaint);
}

/* CASET/PASET/RAMWR failed: that area's pixels are not sent at all. */
static void t_window_fail_drops_area(void)
{
    fail_cmd = 0x2B;                               /* PASET */
    flush(320, 40);
    CHECK(dma_started == 0);                       /* nothing sent to a wrong address */
    CHECK(flush_ready_cnt == 1 && s_need_repaint && s_spi_err == 1);
}

/* The DMA-complete IRQ never comes: the next flush gives up after its bound,
 * aborts, counts it and carries on (no hang for the watchdog). */
static void t_missed_dma_irq(void)
{
    flush(320, 40);
    CHECK(s_dma_busy);
    tick_autoadvance = 1;                          /* time passes while it waits */
    uint32_t t0 = now_ms;
    flush(320, 40);
    tick_autoadvance = 0;
    CHECK(now_ms - t0 >= 100u && now_ms - t0 < 200u);   /* bounded wait */
    CHECK(aborts >= 1 && s_spi_err == 1 && s_need_repaint);
    CHECK(dma_started == 2 && flush_ready_cnt == 2);
}

/* An area bigger than tx_buf is dropped, never written past the buffer. */
static void t_oversized_area_dropped(void)
{
    flush(320, DISP_BUF_LINES + 1);
    CHECK(dma_started == 0 && s_spi_err == 1 && flush_ready_cnt == 1);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "clean_boot",                 t_clean_boot },
        { "good_area",                  t_good_area },
        { "dma_refused_fallback_ok",    t_dma_refused_fallback_ok },
        { "dma_refused_fallback_fails", t_dma_refused_fallback_fails },
        { "three_lost_reinit",          t_three_lost_reinit },
        { "good_area_resets_streak",    t_good_area_resets_streak },
        { "failed_reinit_retried",      t_failed_reinit_retried },
        { "boot_errors_force_reinit",   t_boot_errors_force_reinit },
        { "window_fail_drops_area",     t_window_fail_drops_area },
        { "missed_dma_irq",             t_missed_dma_irq },
        { "oversized_area_dropped",     t_oversized_area_dropped },
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
