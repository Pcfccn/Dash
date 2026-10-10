#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include <stdint.h>

/* ILI9488 SPI module, 4.0" no-touch. Used in portrait: 320 wide x 480 tall
 * (the panel's native orientation). */
#define LCD_H_RES 320
#define LCD_V_RES 480

void lv_port_disp_init(void);
uint16_t lv_port_disp_spi_errors(void);   /* aborted SPI2 transfers since boot */
uint16_t lv_port_disp_recoveries(void);   /* panel re-inits after error runs   */
uint16_t lv_port_disp_dma_fallbacks(void);/* refused DMA starts, sent blocking  */

/* Repair after SPI errors: repaint the screen, or re-init the panel after a
 * run of errors. Call from the loop, outside lv_timer_handler(). */
void lv_port_disp_service(void);

void lv_port_disp_inject_errors(void);   /* FAULT_TEST 3 only (fault.h)        */

#endif /* LV_PORT_DISP_H */
