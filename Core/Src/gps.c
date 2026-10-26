/* GPS link and decoding — see gps.h. No HAL in here: the UART is behind
 * gps_hw.h, so the host tests drive this file with a mock. */

#include "gps.h"
#include "gps_hw.h"
#include <string.h>

static gps_state_t   s;
static nmea_parser_t parser;

static uint32_t state_t0;        /* tick the current link state began         */
static uint32_t heard;           /* valid sentences since then                */
static bool     need_save;       /* rate changed: $PCAS00 once it is heard    */
static uint32_t rate_t0;         /* start of the current one-second window    */
static uint8_t  rate_cnt;

/* "$<body>*hh\r\n" */
static void send_cmd(const char *body)
{
    static const char H[] = "0123456789ABCDEF";
    char line[64];
    size_t n = strlen(body);
    if (n > sizeof line - 7u) return;
    uint8_t cs = nmea_checksum(body);
    line[0] = '$';
    memcpy(&line[1], body, n);
    line[n + 1u] = '*';
    line[n + 2u] = H[cs >> 4];
    line[n + 3u] = H[cs & 15u];
    line[n + 4u] = '\r';
    line[n + 5u] = '\n';
    line[n + 6u] = '\0';
    gps_hw_write(line);
}

static void enter(gps_link_t l, uint32_t now)
{
    uint32_t baud = (l == GPS_LINK_PROBE_SLOW) ? GPS_BAUD_SLOW : GPS_BAUD_FAST;
    if (baud != s.baud) {
        gps_hw_set_baud(baud);
        s.baud = baud;
        parser.in = false;           /* a half line at the old rate is garbage */
    }
    s.link   = l;
    state_t0 = now;
    heard    = 0;
}

void gps_init(void)
{
    memset(&s, 0, sizeof s);
    nmea_init(&parser, &s.fix);
    need_save = false;
    rate_cnt = 0;
    gps_hw_init(GPS_BAUD_FAST);
    s.baud = GPS_BAUD_FAST;
    uint32_t now = gps_hw_now_ms();
    rate_t0 = now;
    enter(GPS_LINK_PROBE_FAST, now);
}

static void link_step(uint32_t now)
{
    switch (s.link) {
    case GPS_LINK_PROBE_FAST:
        if (heard) {
            send_cmd("PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0,,,,0");   /* GGA + RMC only */
            send_cmd("PCAS02,100");                              /* 10 Hz          */
            if (need_save) { send_cmd("PCAS00"); need_save = false; }
            enter(GPS_LINK_RUN, now);
        } else if (now - state_t0 >= GPS_PROBE_MS) {
            enter(GPS_LINK_PROBE_SLOW, now);
        }
        break;
    case GPS_LINK_PROBE_SLOW:
        if (heard) {
            send_cmd("PCAS01,5");                                /* -> 115200      */
            need_save = true;
            enter(GPS_LINK_PROBE_FAST, now);
        } else if (now - state_t0 >= GPS_PROBE_MS) {
            enter(GPS_LINK_ABSENT, now);
        }
        break;
    case GPS_LINK_RUN:
        if (now - s.rx_ms >= GPS_LOST_MS) enter(GPS_LINK_PROBE_FAST, now);
        break;
    case GPS_LINK_ABSENT:
    default:
        if (now - state_t0 >= GPS_ABSENT_MS) enter(GPS_LINK_PROBE_FAST, now);
        break;
    }
}

void gps_poll(void)
{
    int c;
    while ((c = gps_hw_getc()) >= 0) {
        nmea_result_t r = nmea_feed(&parser, &s.fix, (char)c);
        if (r == NMEA_NONE || r == NMEA_BAD) continue;
        /* A sentence ends on the line end just read: its receive time (IRQ),
         * not the drain time, or one that waited out a stall looks fresh
         * (review C7 SW-06; the same rule as the CAN frames). */
        uint32_t at = gps_hw_line_ms();
        s.rx_ms = at;
        heard++;
        if (r == NMEA_RMC) {
            if (rate_cnt < 255u) rate_cnt++;
            s.rmc_ms = at;
            if (s.fix.rmc_fix) s.fix_ms = at;
        } else if (r == NMEA_GGA) {
            s.gga_ms = at;
        }
    }
    uint32_t now = gps_hw_now_ms();          /* after the drain: no stamp is later */
    s.ok = parser.ok;
    s.bad = parser.bad;
    s.overruns = gps_hw_overruns();
    if (now - rate_t0 >= 1000u) {
        s.rate_hz = rate_cnt;
        rate_cnt = 0;
        rate_t0 = now;
    }
    link_step(now);
}

const gps_state_t *gps_state(void) { return &s; }

bool gps_fix_fresh(uint32_t now)
{
    return s.fix.rmc_fix && s.fix_ms != 0u && now - s.fix_ms < GPS_STALE_MS;
}

bool gps_gga_fresh(uint32_t t)
{
    int32_t age = (int32_t)(t - s.gga_ms);          /* a GGA may follow the RMC */
    return s.gga_ms != 0u && age < (int32_t)GPS_STALE_MS && age > -(int32_t)GPS_STALE_MS;
}
