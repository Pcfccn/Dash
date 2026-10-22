/* spi_nor.h — the W25Q64 SPI NOR on the WeAct MiniSTM32H743 board.
 *
 *   SPI1: PB3 = SCK, PB4 = MISO, PD7 = MOSI (AF5), PD6 = CS (GPIO)
 *   (Zephyr board file weact/mini_stm32h743: jedec-id ef 40 17, 40 MHz max)
 *
 * Hand-written HAL init, not in Dash.ioc — like the ST7735 (SPI4) and the GPS
 * (USART2) — so a CubeMX regeneration cannot touch it. Blocking only for the
 * few bytes of a command; program and erase are started here and finish in
 * the chip while the loop goes on (nor_busy() polls the status register), so
 * a 4 KB erase (45 ms typical, 400 ms worst case) never stalls the superloop.
 * Every transfer has a bounded timeout. Used by nvlog.c. */
#ifndef SPI_NOR_H
#define SPI_NOR_H

#include <stdbool.h>
#include <stdint.h>

#define NOR_SIZE     (8u * 1024u * 1024u)
#define NOR_SECTOR   4096u           /* smallest erase unit           */
#define NOR_PAGE     256u            /* a program may not cross one   */

/* SPI1 + chip wake-up + JEDEC ID check. False (and every other call then
 * fails) when the chip does not answer as a Winbond-style 25-series NOR of
 * at least 8 MB. */
bool     nor_init(void);
uint32_t nor_jedec(void);            /* 0xEF4017 for the W25Q64; 0 = none */

bool nor_read(uint32_t addr, void *buf, uint32_t len);
bool nor_busy(void);                 /* a program / erase is still running */
/* Start a page program: len <= NOR_PAGE and inside one page. False when the
 * chip is busy or the transfer failed. */
bool nor_program(uint32_t addr, const void *buf, uint32_t len);
bool nor_erase4k(uint32_t addr);     /* start a 4 KB sector erase */

#endif /* SPI_NOR_H */
