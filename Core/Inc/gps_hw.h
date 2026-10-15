#ifndef GPS_HW_H
#define GPS_HW_H

/* USART2 for the GPS receiver, register level (gps_hw.c). Hand-written rather
 * than through Dash.ioc, like the ST7735's SPI4: nothing here depends on a
 * CubeMX regeneration, and the link logic in gps.c stays testable on the host
 * against a mock of these functions. */

#include <stdint.h>

void     gps_hw_init(uint32_t baud);     /* pins, clock, RX interrupt         */
void     gps_hw_set_baud(uint32_t baud); /* drops anything half-received      */
void     gps_hw_write(const char *s);    /* blocking until the last bit left  */
int      gps_hw_getc(void);              /* next received byte, -1 if none    */
uint32_t gps_hw_line_ms(void);          /* receive tick of the last '\r'/'\n' gps_hw_getc() returned */
uint16_t gps_hw_overruns(void);          /* bytes lost (UART or ring)         */
uint32_t gps_hw_now_ms(void);            /* HAL tick                           */

#endif /* GPS_HW_H */
