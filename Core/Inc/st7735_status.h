#ifndef ST7735_STATUS_H
#define ST7735_STATUS_H

#include <stdint.h>

/* Small on-board 0.96" ST7735 (160x80) status display on the WeAct
 * MiniSTM32H743 board. Separate hardware from the 4" ILI9488 dashboard:
 * its own SPI4 bus + GPIOs, shown as a second LVGL display.
 *
 * Call once, after lv_port_disp_init() (i.e. after lv_init() has run). */
void st7735_status_init(void);

/* Link line: the bus carrying frames is not the same as the ECM answering
 * (broadcasts alone keep the bus up). */
typedef enum {
    ST_LINK_DOWN = 0,           /* no frames: "CAN --" (red)                    */
    ST_LINK_BUS,                /* frames, but no fresh ECM data: "NO ECM" (amber) */
    ST_LINK_ECM                 /* ECM answering: "ECM OK" (green)              */
} st_link_t;

/* Refresh the status readout. Safe to call from the LVGL task; throttle the
 * caller (a few Hz is plenty). Values < 0 = unknown/stale, shown as "--";
 * batt_dv is the battery in 0.1 V. Only changed labels are redrawn. */
void st7735_status_set(int32_t speed_kmh, int32_t rpm, int32_t batt_dv, st_link_t link);

/* Diagnostic line: current cluster page, live KEY(PC13) level, press count.
 * Lets us see whether the page button is wired/seen. */
void st7735_status_key_dbg(uint8_t page, uint8_t key_raw, uint32_t key_cnt);

/* Backlight overlay: when the KEY gesture puts us in brightness-control mode,
 * show the current backlight level big on the small screen; hide it otherwise.
 *   mode: 0 = pages mode (hide overlay), 1 = brightness mode (show "BL xx%")
 *   pct : current backlight duty in % */
void st7735_status_backlight(uint8_t mode, uint8_t pct);
uint16_t st7735_status_spi_errors(void);  /* aborted SPI4 transfers since boot */

#endif /* ST7735_STATUS_H */
