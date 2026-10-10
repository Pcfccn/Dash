#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include <stdint.h>

/* ILI9488 SPI module, 4.0" no-touch. Used in portrait: 320 wide x 480 tall
 * (the panel's native orientation). */
#define LCD_H_RES 320
#define LCD_V_RES 480

void lv_port_disp_init(void);
uint16_t lv_port_disp_spi_errors(void);   /* aborted SPI2 transfers since boot */

#endif /* LV_PORT_DISP_H */
