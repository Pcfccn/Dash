#ifndef GPS_H
#define GPS_H

/* GPS receiver (ATGM336H board) on USART2: PA2 = TX -> module RX, PA3 = RX <-
 * module TX, powered from 3V3 (docs/future/build-hardware.md §3 "GPS").
 *
 * The link finds the module by itself: it listens at 115200 baud, then at the
 * factory 9600; at 9600 it switches the module to 115200 ($PCAS01,5) and,
 * once it hears it there, selects RMC + GGA only ($PCAS03), 10 Hz ($PCAS02,100)
 * and saves that ($PCAS00) so the next power-up starts at 115200. Losing the
 * stream for GPS_LOST_MS starts the search again; no module at either rate is
 * retried every GPS_ABSENT_MS. CASIC protocol spec V4.2.0.3 §1.6.
 *
 * Hardware access is in gps_hw.c; this file and gps_nmea.c are pure logic and
 * run in the host tests (tests/host/test_gps.c, test_gps_nmea.c). */

#include <stdbool.h>
#include <stdint.h>
#include "gps_nmea.h"

#define GPS_BAUD_FAST  115200u
#define GPS_BAUD_SLOW    9600u
#define GPS_PROBE_MS     1500u   /* listen this long at a rate for one sentence */
#define GPS_LOST_MS      3000u   /* running: no valid sentence -> search again  */
#define GPS_ABSENT_MS   10000u   /* nothing at either rate: wait, then retry    */
#define GPS_STALE_MS     2000u   /* a fix older than this is not shown          */

typedef enum {
    GPS_LINK_PROBE_FAST = 0,     /* listening at 115200                         */
    GPS_LINK_PROBE_SLOW,         /* listening at 9600                           */
    GPS_LINK_RUN,                /* configured, sentences arriving              */
    GPS_LINK_ABSENT              /* nothing heard; retried later                */
} gps_link_t;

typedef struct {
    nmea_fix_t fix;              /* last RMC / GGA contents                     */
    uint32_t   fix_ms;           /* HAL tick of the last RMC with a fix         */
    uint32_t   rx_ms;            /* HAL tick of the last valid sentence         */
    gps_link_t link;
    uint32_t   baud;             /* our UART rate now                           */
    uint32_t   ok, bad;          /* sentences with a good / bad checksum        */
    uint16_t   overruns;         /* UART bytes lost                             */
    uint8_t    rate_hz;          /* RMC sentences in the last full second       */
} gps_state_t;

void gps_init(void);
void gps_poll(void);             /* from the superloop: drain, decode, link     */
const gps_state_t *gps_state(void);
/* A fix (RMC status A) received within GPS_STALE_MS. */
bool gps_fix_fresh(uint32_t now);

#endif /* GPS_H */
