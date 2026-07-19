#ifndef ST7735_STATUS_H
#define ST7735_STATUS_H

#include <stdint.h>

/* Small on-board 0.96" ST7735 (160x80) status display on the WeAct
 * MiniSTM32H743 board. Separate hardware from the 4" ILI9488 dashboard:
 * its own SPI4 bus + GPIOs, shown as a second LVGL display.
 *
 * Call once, after lv_port_disp_init() (i.e. after lv_init() has run). */
void st7735_status_init(void);

/* Refresh the status readout. Safe to call from the LVGL task; throttle the
 * caller (a few Hz is plenty). */
void st7735_status_set(int32_t speed_kmh, int32_t rpm);

/* Diagnostic line: current cluster page, live KEY(PC13) level, press count.
 * Lets us see whether the page button is wired/seen. */
void st7735_status_key_dbg(uint8_t page, uint8_t key_raw, uint32_t key_cnt);

/* Backlight overlay: when the KEY gesture puts us in brightness-control mode,
 * show the current backlight level big on the small screen; hide it otherwise.
 *   mode: 0 = pages mode (hide overlay), 1 = brightness mode (show "BL xx%")
 *   pct : current backlight duty in % */
void st7735_status_backlight(uint8_t mode, uint8_t pct);

#endif /* ST7735_STATUS_H */
