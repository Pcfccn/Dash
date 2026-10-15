/* Host-side tests for Core/Src/st7735_status.c: SPI4 error handling and the
 * restart + repaint repair (review C7 SW-01), and that the 250 ms callers
 * write only changed labels (C7 SW-02), by fault injection into a mocked
 * SPI HAL and LVGL (tests/host/mock_st7735_status/). This checks the driver's
 * decisions, not the panel, the wiring or LVGL's ST7735 driver. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/st7735_status.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ------------------------------------------------------------- mock HAL */
GPIO_TypeDef mock_gpioc, mock_gpioe;

static uint32_t now_ms;
static int tx_calls, tx_fail_next, tx_fail_all;
static int aborts, spi_inits, spi_deinits, spi_init_fail, error_calls;

uint32_t HAL_GetTick(void) { return now_ms; }
void Error_Handler(void) { error_calls++; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s) { (void)p; (void)pin; (void)s; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *i) { (void)p; (void)i; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h)
{
    (void)h; spi_inits++;
    return spi_init_fail ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef *h) { (void)h; spi_deinits++; return HAL_OK; }
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h) { (void)h; aborts++; return HAL_OK; }
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n, uint32_t t)
{
    (void)h; (void)p; (void)n; (void)t;
    tx_calls++;
    if (tx_fail_all) return HAL_TIMEOUT;
    if (tx_fail_next > 0) { tx_fail_next--; return HAL_TIMEOUT; }
    return HAL_OK;
}

/* ------------------------------------------------------------ mock LVGL */
struct lv_obj_t     { int id; };
struct lv_display_t { int id; };
const lv_font_t lv_font_montserrat_12, lv_font_montserrat_14, montserrat_bold_28;

static lv_display_t disp_main = { 1 }, disp_st = { 2 };
static lv_obj_t     objs[32];
static int          nobj;
static lv_obj_t     screen = { 100 };
static lv_st7735_send_cmd_cb_t   cmd_cb;
static lv_st7735_send_color_cb_t color_cb;
static int flush_ready_calls, invalidates, text_writes, flag_writes;
static int init_cmds_tried;            /* start-up commands that reached st_tx */

lv_display_t *lv_display_get_default(void) { return &disp_main; }
void lv_display_set_default(lv_display_t *d) { (void)d; }
void lv_display_set_buffers(lv_display_t *d, void *a, void *b, uint32_t n, int m) { (void)d; (void)a; (void)b; (void)n; (void)m; }
lv_obj_t *lv_display_get_screen_active(lv_display_t *d) { (void)d; return &screen; }
void lv_display_flush_ready(lv_display_t *d) { (void)d; flush_ready_calls++; }
uint32_t lv_tick_get(void) { return now_ms; }
void lv_lock(void) {}
void lv_unlock(void) {}
lv_color_t lv_color_black(void) { return (lv_color_t){ 0, 0, 0 }; }
lv_color_t lv_color_white(void) { return (lv_color_t){ 255, 255, 255 }; }
lv_color_t lv_color_hex(uint32_t c) { return (lv_color_t){ (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }; }
lv_obj_t *lv_obj_create(lv_obj_t *p) { (void)p; return &objs[nobj++]; }
lv_obj_t *lv_label_create(lv_obj_t *p) { (void)p; return &objs[nobj++]; }
void lv_label_set_text(lv_obj_t *o, const char *t) { (void)o; (void)t; text_writes++; }
void lv_label_set_text_fmt(lv_obj_t *o, const char *f, ...) { (void)o; (void)f; text_writes++; }
void lv_obj_remove_style_all(lv_obj_t *o) { (void)o; }
void lv_obj_set_size(lv_obj_t *o, int32_t w, int32_t h) { (void)o; (void)w; (void)h; }
void lv_obj_set_width(lv_obj_t *o, int32_t w) { (void)o; (void)w; }
void lv_obj_align(lv_obj_t *o, int a, int32_t x, int32_t y) { (void)o; (void)a; (void)x; (void)y; }
void lv_obj_add_flag(lv_obj_t *o, uint32_t f) { (void)o; (void)f; flag_writes++; }
void lv_obj_clear_flag(lv_obj_t *o, uint32_t f) { (void)o; (void)f; flag_writes++; }
void lv_obj_invalidate(const lv_obj_t *o) { if (o == &screen) invalidates++; }
void lv_obj_set_style_bg_color(lv_obj_t *o, lv_color_t c, uint32_t s) { (void)o; (void)c; (void)s; }
void lv_obj_set_style_bg_opa(lv_obj_t *o, uint8_t v, uint32_t s) { (void)o; (void)v; (void)s; }
void lv_obj_set_style_pad_all(lv_obj_t *o, int32_t v, uint32_t s) { (void)o; (void)v; (void)s; }
void lv_obj_set_style_text_font(lv_obj_t *o, const lv_font_t *f, uint32_t s) { (void)o; (void)f; (void)s; }
void lv_obj_set_style_text_color(lv_obj_t *o, lv_color_t c, uint32_t s) { (void)o; (void)c; (void)s; }
void lv_obj_set_style_text_align(lv_obj_t *o, int a, uint32_t s) { (void)o; (void)a; (void)s; }

/* Like LVGL's: the generic MIPI start-up commands go out from inside create. */
static const uint8_t INIT_CMDS[] = { 0x01, 0x11, 0x13, 0x36, 0x3A, 0x29 };
lv_display_t *lv_st7735_create(uint32_t w, uint32_t h, lv_lcd_flag_t f,
                               lv_st7735_send_cmd_cb_t c, lv_st7735_send_color_cb_t p)
{
    (void)w; (void)h; (void)f;
    cmd_cb = c; color_cb = p;
    static const uint8_t param = 0x05;
    for (size_t i = 0; i < sizeof INIT_CMDS; i++) {
        int before = tx_calls;
        c(&disp_st, &INIT_CMDS[i], 1, &param, 1);
        if (tx_calls != before) init_cmds_tried++;
    }
    return &disp_st;
}
void lv_st7735_set_gap(lv_display_t *d, uint16_t x, uint16_t y) { (void)d; (void)x; (void)y; }
void lv_st7735_set_invert(lv_display_t *d, bool inv) { (void)d; (void)inv; }

/* ------------------------------------------------------------ harness */
static void reset(void)
{
    now_ms = 5000;
    tx_calls = tx_fail_next = tx_fail_all = 0;
    aborts = spi_inits = spi_deinits = spi_init_fail = error_calls = 0;
    nobj = 0; flush_ready_calls = invalidates = text_writes = flag_writes = 0;
    init_cmds_tried = 0;
    status_disp = NULL;
    st_spi_err = 0; st_err_seq = 0; st_fixed_seq = 0; st_fix_ms = 0;
    st_recover_cnt = 0; st_ready = false;
}

/* One LVGL flush of a 4-pixel area: window commands, then pixels. */
static void flush_area(void)
{
    static const uint8_t caset = 0x2A, raset = 0x2B, ramwr = 0x2C;
    static const uint8_t win[4] = { 0, 0, 0, 3 };
    uint8_t px[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    cmd_cb(&disp_st, &caset, 1, win, sizeof win);
    cmd_cb(&disp_st, &raset, 1, win, sizeof win);
    color_cb(&disp_st, &ramwr, 1, px, sizeof px);
}

/* ------------------------------------------------------------ tests */
static void t_clean_init_owes_nothing(void)
{
    st7735_status_init();
    CHECK(init_cmds_tried == (int)sizeof INIT_CMDS);
    CHECK(error_calls == 0 && st_ready);
    int inits = spi_inits;
    st7735_status_service(now_ms);
    CHECK(spi_inits == inits && invalidates == 0 && st7735_status_recoveries() == 0);
    int tx = tx_calls;
    flush_area();
    CHECK(tx_calls == tx + 6 && flush_ready_calls == 1);   /* 3 cmds + 3 params/pixels */
}

/* A glitch in the start-up commands must not drop the ones after it. */
static void t_init_error_does_not_skip_later_commands(void)
{
    tx_fail_next = 1;                                     /* SWRESET fails */
    st7735_status_init();
    CHECK(init_cmds_tried == (int)sizeof INIT_CMDS);
    CHECK(st7735_status_spi_errors() == 1);
    st7735_status_service(now_ms);                        /* owed: repaired at once */
    CHECK(spi_deinits == 1 && invalidates == 1 && st7735_status_recoveries() == 1);
}

static void t_error_holds_traffic_until_repair(void)
{
    st7735_status_init();
    tx_fail_next = 1;
    flush_area();                                         /* CASET fails */
    CHECK(st7735_status_spi_errors() == 1 && aborts == 1);
    CHECK(flush_ready_calls == 1);                        /* LVGL is never left waiting */
    int tx = tx_calls;
    flush_area();
    flush_area();
    CHECK(tx_calls == tx);                                /* held: no 50 ms timeouts */
    CHECK(flush_ready_calls == 3);
    st7735_status_service(now_ms);
    CHECK(spi_deinits == 1 && invalidates == 1);
    flush_area();
    CHECK(tx_calls == tx + 6);                            /* flowing again */
}

/* The rest of a command is not sent after its first byte failed. */
static void t_failed_command_drops_its_params(void)
{
    st7735_status_init();
    int tx = tx_calls;
    tx_fail_next = 1;
    static const uint8_t caset = 0x2A, win[4] = { 0 };
    cmd_cb(&disp_st, &caset, 1, win, sizeof win);
    CHECK(tx_calls == tx + 1);
    st7735_status_service(now_ms);
    tx = tx_calls;
    tx_fail_next = 1;
    uint8_t px[4] = { 0 };
    static const uint8_t ramwr = 0x2C;
    color_cb(&disp_st, &ramwr, 1, px, sizeof px);
    CHECK(tx_calls == tx + 1 && flush_ready_calls == 1);
}

static void t_repair_at_most_once_per_second(void)
{
    st7735_status_init();
    tx_fail_all = 1;
    flush_area();
    st7735_status_service(now_ms);
    CHECK(st7735_status_recoveries() == 1 && invalidates == 1);
    flush_area();                                         /* the repaint fails too */
    now_ms += ST_RECOVER_MIN_MS - 1u;
    st7735_status_service(now_ms);
    CHECK(st7735_status_recoveries() == 1);
    now_ms += 1u;
    st7735_status_service(now_ms);
    CHECK(st7735_status_recoveries() == 2 && invalidates == 2);
    /* a broken SPI4 costs one timeout per repair, not one per transfer */
    int tx = tx_calls;
    for (int i = 0; i < 10; i++) flush_area();
    CHECK(tx_calls == tx + 1);
}

/* SPI4 refuses to start again: no repaint, still owed, retried later. */
static void t_failed_restart_is_retried(void)
{
    st7735_status_init();
    tx_fail_next = 1;
    flush_area();
    spi_init_fail = 1;
    st7735_status_service(now_ms);
    CHECK(invalidates == 0 && st_broken());
    CHECK(error_calls == 0);                              /* not a reason to reset the board */
    spi_init_fail = 0;
    now_ms += ST_RECOVER_MIN_MS;
    st7735_status_service(now_ms);
    CHECK(invalidates == 1 && !st_broken());
}

/* Counters keep counting past the DIAG counter's saturation. */
static void t_saturated_counter_still_repairs(void)
{
    st7735_status_init();
    st7735_status_service(now_ms);
    st_spi_err = 0xFFFFu;
    tx_fail_next = 1;
    flush_area();
    CHECK(st7735_status_spi_errors() == 0xFFFFu && st_broken());
    st7735_status_service(now_ms);
    CHECK(invalidates == 1);
}

static void t_key_dbg_writes_only_changes(void)
{
    st7735_status_init();
    int w = text_writes;
    st7735_status_key_dbg(1, 0, 7);
    CHECK(text_writes == w + 1);
    st7735_status_key_dbg(1, 0, 7);
    st7735_status_key_dbg(1, 0, 7);
    CHECK(text_writes == w + 1);
    st7735_status_key_dbg(1, 1, 7);
    st7735_status_key_dbg(2, 1, 7);
    st7735_status_key_dbg(2, 1, 8);
    CHECK(text_writes == w + 4);
}

static void t_backlight_writes_only_changes(void)
{
    st7735_status_init();
    int w = text_writes, f = flag_writes;
    st7735_status_backlight(0, 100, false);               /* pages mode: hide once */
    st7735_status_backlight(0, 100, false);
    CHECK(text_writes == w && flag_writes == f + 1);
    st7735_status_backlight(1, 100, false);               /* caption + level + show */
    CHECK(text_writes == w + 2 && flag_writes == f + 2);
    st7735_status_backlight(1, 100, false);
    st7735_status_backlight(1, 100, false);
    CHECK(text_writes == w + 2 && flag_writes == f + 2);
    st7735_status_backlight(1, 90, false);                /* level only */
    CHECK(text_writes == w + 3 && flag_writes == f + 2);
    st7735_status_backlight(1, 90, true);                 /* caption only */
    CHECK(text_writes == w + 4);
    st7735_status_backlight(0, 90, true);
    st7735_status_backlight(0, 90, true);
    CHECK(flag_writes == f + 3);
}

int main(void)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "clean_init_owes_nothing",     t_clean_init_owes_nothing },
        { "init_error_keeps_later_cmds", t_init_error_does_not_skip_later_commands },
        { "error_holds_traffic",         t_error_holds_traffic_until_repair },
        { "failed_cmd_drops_params",     t_failed_command_drops_its_params },
        { "repair_once_per_second",      t_repair_at_most_once_per_second },
        { "failed_restart_retried",      t_failed_restart_is_retried },
        { "saturated_counter_repairs",   t_saturated_counter_still_repairs },
        { "key_dbg_only_changes",        t_key_dbg_writes_only_changes },
        { "backlight_only_changes",      t_backlight_writes_only_changes },
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
