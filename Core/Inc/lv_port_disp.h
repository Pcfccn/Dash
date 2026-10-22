#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include <stdint.h>

/* ILI9488 SPI module, 4.0" no-touch. Used in portrait: 320 wide x 480 tall
 * (the panel's native orientation). */
#define LCD_H_RES 320
#define LCD_V_RES 480

/* 1 = picture turned 180 degrees, for a module mounted upside down. A TN
 * panel looks worse from one side of its vertical axis (on similar panels:
 * from below); turning it over moves that side away from the driver. */
#ifndef LCD_FLIP
#define LCD_FLIP 0
#endif

/* 1 = at power-up, show three bands sent as 16-bit pixels for 4 s before the
 * normal start: tells which controller the module really has (lv_port_disp.c,
 * lcd_probe_565). Bench only, does no harm. */
#ifndef LCD_PROBE
#define LCD_PROBE 0
#endif

void lv_port_disp_init(void);
uint16_t lv_port_disp_spi_errors(void);   /* aborted SPI2 transfers since boot */
uint16_t lv_port_disp_recoveries(void);   /* panel re-inits after error runs   */
uint16_t lv_port_disp_dma_fallbacks(void);/* refused DMA starts, sent blocking  */

/* Repair after SPI errors: repaint the screen, or re-init the panel after a
 * run of errors. Call from the loop, outside lv_timer_handler(). */
void lv_port_disp_service(void);

void lv_port_disp_inject_errors(void);   /* FAULT_TEST 3 only (fault.h)        */

#endif /* LV_PORT_DISP_H */
