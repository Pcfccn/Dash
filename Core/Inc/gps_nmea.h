#ifndef GPS_NMEA_H
#define GPS_NMEA_H

/* NMEA 0183 sentence parser for the GPS receiver (ATGM336H, CASIC firmware).
 * Pure C, no HAL: bytes in, decoded RMC / GGA out, so the host tests run it
 * as is (tests/host/test_gps_nmea.c). Any talker ID is accepted (GP, GN, BD,
 * GB, GL, GA ...): a multi-GNSS receiver reports its combined fix as GN.
 *
 * Validation before anything is written, as on the CAN side: a sentence
 * counts only with a correct "*hh" checksum, and an RMC / GGA field that does
 * not parse leaves the previous values alone rather than turning into 0. */

#include <stdbool.h>
#include <stdint.h>

#define NMEA_LINE_MAX 96u            /* NMEA allows 82 incl. "$" and CR LF */

/* What the last RMC and GGA said. Floats are NaN until first known. */
typedef struct {
    /* RMC */
    bool     rmc_fix;            /* status A and mode not N                    */
    float    speed_kmh;          /* over ground (from knots); NaN = unknown    */
    float    course_deg;         /* true; NaN when the receiver leaves it empty */
    double   lat_deg, lon_deg;   /* + north / east; NaN without a fix          */
    uint8_t  hh, mm, ss;         /* UTC of the last RMC                        */
    uint16_t ms;
    uint8_t  day, month;         /* UTC date, 0 = not known yet                */
    uint16_t year;               /* e.g. 2026                                  */
    /* GGA */
    uint8_t  quality;            /* 0 no fix, 1 GPS, 2 DGPS, 6 dead reckoning  */
    uint8_t  sats;               /* satellites in use                          */
    float    hdop;               /* NaN = unknown                              */
    float    alt_m;              /* above mean sea level; NaN = unknown        */
} nmea_fix_t;

typedef struct {
    char     buf[NMEA_LINE_MAX];
    uint8_t  len;
    bool     in;                 /* between "$" and the line end               */
    uint32_t ok, bad;            /* complete sentences: checksum good / not    */
} nmea_parser_t;

typedef enum {
    NMEA_NONE = 0,               /* no complete sentence yet                   */
    NMEA_RMC,                    /* fix updated from an RMC                    */
    NMEA_GGA,                    /* fix updated from a GGA                     */
    NMEA_OTHER,                  /* valid sentence of another type             */
    NMEA_BAD                     /* bad / missing checksum, too long, garbled  */
} nmea_result_t;

void nmea_init(nmea_parser_t *p, nmea_fix_t *fix);
/* Feed one received byte; returns the result when it completes a sentence. */
nmea_result_t nmea_feed(nmea_parser_t *p, nmea_fix_t *fix, char c);
/* "*hh" checksum of a sentence body (the text between "$" and "*"). */
uint8_t nmea_checksum(const char *body);

#endif /* GPS_NMEA_H */
