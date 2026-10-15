/* Host-side stand-in for LVGL's ST7735 driver header (same callback shapes).
 * Implemented by tests/host/test_st7735_status.c. */
#ifndef MOCK_LV_ST7735_H
#define MOCK_LV_ST7735_H

#include "lvgl.h"

typedef uint32_t lv_lcd_flag_t;
#define LV_LCD_FLAG_MIRROR_X 0x1u
#define LV_LCD_FLAG_MIRROR_Y 0x2u
#define LV_LCD_FLAG_BGR      0x8u

typedef void (*lv_st7735_send_cmd_cb_t)(lv_display_t *d, const uint8_t *cmd, size_t cmd_size,
                                        const uint8_t *param, size_t param_size);
typedef void (*lv_st7735_send_color_cb_t)(lv_display_t *d, const uint8_t *cmd, size_t cmd_size,
                                          uint8_t *param, size_t param_size);

lv_display_t *lv_st7735_create(uint32_t hor_res, uint32_t ver_res, lv_lcd_flag_t flags,
                               lv_st7735_send_cmd_cb_t send_cmd_cb,
                               lv_st7735_send_color_cb_t send_color_cb);
void lv_st7735_set_gap(lv_display_t *d, uint16_t x, uint16_t y);
void lv_st7735_set_invert(lv_display_t *d, bool invert);

#endif
