/* Second, small on-board 0.96" ST7735 (160x80) status display on the WeAct
 * MiniSTM32H743 board, driven through LVGL's built-in ST7735 driver.
 *
 * This is fully independent of the 4" ILI9488 dashboard: its own SPI4 bus and
 * its own GPIOs, all set up here directly via HAL (NOT via CubeMX/.ioc), so it
 * stays self-contained and never touches the main project configuration.
 *
 * Pinout is fixed by the WeAct board (matches their 03-LCD_Test reference):
 *     SPI4:  SCK = PE12,  MOSI/SDA = PE14   (write-only, no MISO routed)
 *     CS = PE11,  DC/RS = PE13
 *     RST: none on this board -> software reset; backlight: always on.
 *
 * The panel is the common 0.96" 160x80 IPS type: native 80(w) x 160(h) with the
 * visible window offset into GRAM by (26, 1), display inversion on, BGR order.
 * If the image is shifted, colours look swapped/negative, or it's upside down,
 * the four knobs to tune are marked "TUNE" below.
 */

#include "st7735_status.h"
#include "main.h"   /* STM32 HAL + GPIO */
#include "lvgl.h"
#include "fdcan_obd.h"    /* g_obd (CAN status + battery) */
#include "src/drivers/display/st7735/lv_st7735.h"
#include <math.h>

LV_FONT_DECLARE(montserrat_bold_28);   /* big bold speed number */

/* ------- board wiring (SPI4 on GPIOE) ------- */
#define ST_GPIO_PORT   GPIOE
#define ST_SCK_PIN     GPIO_PIN_12
#define ST_MOSI_PIN    GPIO_PIN_14
#define ST_CS_PIN      GPIO_PIN_11
#define ST_DC_PIN      GPIO_PIN_13
#define ST_BL_PIN      GPIO_PIN_10   /* backlight gate. Per the WeAct schematic this drives a SI2301
                                      * P-channel MOSFET (3V3 -> FET -> 22R -> LEDA), so it is
                                      * ACTIVE-LOW: drive LOW to turn the backlight ON. */

/* ------- panel geometry (TUNE if the image is shifted) ------- */
#define ST_HOR_RES     80
#define ST_VER_RES     160
#define ST_X_GAP       26
#define ST_Y_GAP       1

static SPI_HandleTypeDef hspi4_st;
static lv_display_t *status_disp;
static lv_obj_t *lbl_spcap, *lbl_speed, *lbl_rpm, *lbl_can, *lbl_canid, *lbl_uptime, *lbl_dbg;
static lv_obj_t *bl_panel, *bl_cap, *bl_val;   /* brightness overlay (shown only in BL mode) */

/* Partial draw buffer (RGB565). 80 x 40 px is plenty for a text screen. */
static uint8_t st_buf[ST_HOR_RES * 40 * 2];

static inline void st_cs_low(void)  { HAL_GPIO_WritePin(ST_GPIO_PORT, ST_CS_PIN, GPIO_PIN_RESET); }
static inline void st_cs_high(void) { HAL_GPIO_WritePin(ST_GPIO_PORT, ST_CS_PIN, GPIO_PIN_SET); }
static inline void st_dc_cmd(void)  { HAL_GPIO_WritePin(ST_GPIO_PORT, ST_DC_PIN, GPIO_PIN_RESET); }
static inline void st_dc_data(void) { HAL_GPIO_WritePin(ST_GPIO_PORT, ST_DC_PIN, GPIO_PIN_SET); }

static void st_hw_init(void)
{
    GPIO_InitTypeDef gi = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SPI4_CLK_ENABLE();

    /* SCK + MOSI as SPI4 alternate function */
    gi.Pin = ST_SCK_PIN | ST_MOSI_PIN;
    gi.Mode = GPIO_MODE_AF_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = GPIO_AF5_SPI4;
    HAL_GPIO_Init(ST_GPIO_PORT, &gi);

    /* CS + DC + backlight gate as plain push-pull outputs */
    gi.Pin = ST_CS_PIN | ST_DC_PIN | ST_BL_PIN;
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = 0;
    HAL_GPIO_Init(ST_GPIO_PORT, &gi);
    st_cs_high();
    /* backlight ON = LOW (P-FET gate, see schematic note above) */
    HAL_GPIO_WritePin(ST_GPIO_PORT, ST_BL_PIN, GPIO_PIN_RESET);

    /* SPI4: master, mode 0 (CPOL=0/CPHA=0 -- standard for ST7735), 8-bit MSB.
     * SPI4 kernel clock is APB2 (~112 MHz); /16 -> ~7 MHz, safe and fast enough. */
    hspi4_st.Instance = SPI4;
    hspi4_st.Init.Mode = SPI_MODE_MASTER;
    /* half-duplex, single data line on MOSI (PE14) -- matches WeAct's proven
     * LCD_Test config; full-duplex (2LINES) leaves RX unread and can stall the
     * large pixel transfers on H7, giving a blank panel. */
    hspi4_st.Init.Direction = SPI_DIRECTION_1LINE;
    hspi4_st.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi4_st.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi4_st.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi4_st.Init.NSS = SPI_NSS_SOFT;
    hspi4_st.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi4_st.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi4_st.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi4_st.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi4_st.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi4_st) != HAL_OK) {
        Error_Handler();
    }
}

/* Bounded like the main panel's (lv_port_disp.c): a stuck SPI4 aborts and
 * counts instead of hanging the loop. 6400 bytes at ~7 MHz take ~7 ms. */
#define ST_SPI_TIMEOUT_MS 50u
static volatile uint16_t st_spi_err;

uint16_t st7735_status_spi_errors(void) { return st_spi_err; }

static void st_tx(const uint8_t *p, size_t len)
{
    if (HAL_SPI_Transmit(&hspi4_st, (uint8_t *)p, (uint16_t)len, ST_SPI_TIMEOUT_MS) != HAL_OK) {
        HAL_SPI_Abort(&hspi4_st);
        if (st_spi_err < 0xFFFFu) st_spi_err++;
    }
}

/* LVGL calls this to send init/setup commands (DC low = command, DC high = params). */
static void st_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                        const uint8_t *param, size_t param_size)
{
    LV_UNUSED(disp);
    st_cs_low();
    st_dc_cmd();
    st_tx(cmd, cmd_size);
    if (param && param_size) {
        st_dc_data();
        st_tx(param, param_size);
    }
    st_cs_high();
}

/* LVGL calls this to push rendered pixels. LVGL stores RGB565 little-endian but
 * the ST7735 wants the high byte first, so swap each pixel's two bytes. */
static void st_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                          uint8_t *param, size_t param_size)
{
    for (size_t i = 0; i + 1 < param_size; i += 2) {
        uint8_t t = param[i];
        param[i] = param[i + 1];
        param[i + 1] = t;
    }
    st_cs_low();
    st_dc_cmd();
    st_tx(cmd, cmd_size);
    st_dc_data();
    st_tx(param, param_size);
    st_cs_high();
    lv_display_flush_ready(disp);
}

void st7735_status_init(void)
{
    /* Remember the current default (the 4" dashboard) -- creating a second
     * display makes it the new default, and we want to restore the 4". */
    lv_display_t *prev = lv_display_get_default();

    st_hw_init();

    status_disp = lv_st7735_create(ST_HOR_RES, ST_VER_RES,
                                   /* TUNE orientation/colour: */
                                   LV_LCD_FLAG_MIRROR_X | LV_LCD_FLAG_MIRROR_Y | LV_LCD_FLAG_BGR,
                                   st_send_cmd, st_send_color);
    lv_st7735_set_gap(status_disp, ST_X_GAP, ST_Y_GAP);
    lv_st7735_set_invert(status_disp, true);   /* TUNE: IPS panels need inversion on */
    lv_display_set_buffers(status_disp, st_buf, NULL, sizeof(st_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* High-contrast layout for in-car daylight readability: pure-black
     * background, pure-white values, bright saturated status colours, and
     * larger speed/rpm digits. No low-contrast greys. */
    lv_obj_t *scr = lv_display_get_screen_active(status_disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 3, 0);

    lbl_spcap = lv_label_create(scr);   /* "SPEED" caption */
    lv_obj_set_style_text_font(lbl_spcap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_spcap, lv_color_hex(0xe8edf3), 0);
    lv_label_set_text(lbl_spcap, "SPEED km/h");
    lv_obj_align(lbl_spcap, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_speed = lv_label_create(scr);   /* big speed number */
    lv_obj_set_style_text_font(lbl_speed, &montserrat_bold_28, 0);
    lv_obj_set_style_text_color(lbl_speed, lv_color_white(), 0);
    lv_label_set_text(lbl_speed, "--");
    lv_obj_align(lbl_speed, LV_ALIGN_TOP_LEFT, 0, 12);

    lbl_rpm = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_rpm, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_rpm, lv_color_white(), 0);
    lv_label_set_text(lbl_rpm, "RPM --");
    lv_obj_align(lbl_rpm, LV_ALIGN_TOP_LEFT, 0, 46);

    lbl_can = lv_label_create(scr);   /* CAN link status (bright green/red) */
    lv_obj_set_style_text_font(lbl_can, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_can, lv_color_hex(0x37d67a), 0);
    lv_label_set_text(lbl_can, "CAN --");
    lv_obj_align(lbl_can, LV_ALIGN_TOP_LEFT, 0, 72);

    lbl_canid = lv_label_create(scr);   /* battery voltage, pure white */
    lv_obj_set_style_text_font(lbl_canid, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_canid, lv_color_white(), 0);
    lv_label_set_text(lbl_canid, "B --");
    lv_obj_align(lbl_canid, LV_ALIGN_TOP_LEFT, 0, 94);

    lbl_uptime = lv_label_create(scr);   /* uptime = liveness heartbeat */
    lv_obj_set_style_text_font(lbl_uptime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_uptime, lv_color_hex(0xd8dee6), 0);
    lv_label_set_text(lbl_uptime, "t 0s");
    lv_obj_align(lbl_uptime, LV_ALIGN_TOP_LEFT, 0, 116);

    lbl_dbg = lv_label_create(scr);   /* page + KEY(PC13) diagnostic line */
    lv_obj_set_style_text_font(lbl_dbg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dbg, lv_color_hex(0xffc033), 0);
    lv_label_set_text(lbl_dbg, "P0 KEY0 #0");
    lv_obj_align(lbl_dbg, LV_ALIGN_TOP_LEFT, 0, 138);

    /* Brightness overlay: a full-screen black panel with a caption and a big
     * number, hidden until the KEY gesture switches into brightness mode. */
    bl_panel = lv_obj_create(scr);
    lv_obj_remove_style_all(bl_panel);
    lv_obj_set_size(bl_panel, ST_HOR_RES, ST_VER_RES);
    lv_obj_align(bl_panel, LV_ALIGN_TOP_LEFT, -3, -3);   /* cancel scr's 3px pad */
    lv_obj_set_style_bg_color(bl_panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bl_panel, LV_OPA_COVER, 0);
    lv_obj_add_flag(bl_panel, LV_OBJ_FLAG_HIDDEN);

    bl_cap = lv_label_create(bl_panel);
    lv_obj_set_style_text_font(bl_cap, &lv_font_montserrat_12, 0);   /* 14 overflowed the 80px width */
    lv_obj_set_style_text_color(bl_cap, lv_color_hex(0xffc033), 0);
    lv_obj_set_width(bl_cap, ST_HOR_RES);                            /* full panel width, so it centers/ */
    lv_obj_set_style_text_align(bl_cap, LV_TEXT_ALIGN_CENTER, 0);    /* wraps instead of clipping off-screen */
    lv_label_set_text(bl_cap, "BRIGHTNESS");
    lv_obj_align(bl_cap, LV_ALIGN_CENTER, 0, -30);

    bl_val = lv_label_create(bl_panel);
    lv_obj_set_style_text_font(bl_val, &montserrat_bold_28, 0);
    lv_obj_set_style_text_color(bl_val, lv_color_white(), 0);
    lv_obj_set_width(bl_val, ST_HOR_RES);                            /* center "100%" within the panel */
    lv_obj_set_style_text_align(bl_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(bl_val, "--");
    lv_obj_align(bl_val, LV_ALIGN_CENTER, 0, 6);

    if (prev) {
        lv_display_set_default(prev);   /* keep the 4" dashboard as the default display */
    }
}

void st7735_status_set(int32_t speed_kmh, int32_t rpm)
{
    if (status_disp == NULL) {
        return;
    }
    lv_lock();
    /* negative = unknown: the caller has no fresh value */
    if (speed_kmh >= 0) lv_label_set_text_fmt(lbl_speed, "%d", (int)speed_kmh);
    else                lv_label_set_text(lbl_speed, "--");
    if (rpm >= 0)       lv_label_set_text_fmt(lbl_rpm, "RPM %d", (int)rpm);
    else                lv_label_set_text(lbl_rpm, "RPM --");
    lv_label_set_text_fmt(lbl_uptime, "t %us", (unsigned)(lv_tick_get() / 1000u));
    lv_label_set_text(lbl_can, g_obd.can_ok ? "CAN OK" : "CAN --");
    lv_obj_set_style_text_color(lbl_can, lv_color_hex(g_obd.can_ok ? 0x37d67a : 0xff2d2d), 0);
    /* battery is NaN until the PID actually answers; casting that to int is UB,
     * so test before converting rather than relying on it landing on 0 */
    float bv = g_obd.battery;
    int bmv = isnan(bv) ? 0 : (int)(bv * 10.0f + 0.5f);   /* 0.1 V steps */
    if (bmv > 10) lv_label_set_text_fmt(lbl_canid, "B %d.%dV", bmv / 10, bmv % 10);
    else          lv_label_set_text(lbl_canid, "B --");
    lv_unlock();
}

void st7735_status_key_dbg(uint8_t page, uint8_t key_raw, uint32_t key_cnt)
{
    if (status_disp == NULL) {
        return;
    }
    lv_lock();
    lv_label_set_text_fmt(lbl_dbg, "P%u KEY%u #%lu",
                          (unsigned)page, (unsigned)key_raw, (unsigned long)key_cnt);
    lv_unlock();
}

void st7735_status_backlight(uint8_t mode, uint8_t pct)
{
    if (status_disp == NULL) {
        return;
    }
    lv_lock();
    if (mode) {
        lv_label_set_text_fmt(bl_val, "%u%%", (unsigned)pct);
        lv_obj_clear_flag(bl_panel, LV_OBJ_FLAG_HIDDEN);   /* overlay on top */
    } else {
        lv_obj_add_flag(bl_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_unlock();
}
