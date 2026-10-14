/* Host-side tests for Core/Src/gps_nmea.c: checksum, field validation, RMC /
 * GGA decoding, resync after garbage. The real source is #included. */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../Core/Src/gps_nmea.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static nmea_parser_t P;
static nmea_fix_t    F;

/* Feed "$<body>*hh\r\n" with the correct checksum; returns the last result. */
static nmea_result_t feed_body(const char *body)
{
    char line[160];
    snprintf(line, sizeof line, "$%s*%02X\r\n", body, nmea_checksum(body));
    nmea_result_t r = NMEA_NONE, last = NMEA_NONE;
    for (const char *c = line; *c; c++) if ((r = nmea_feed(&P, &F, *c)) != NMEA_NONE) last = r;
    return last;
}
static nmea_result_t feed_raw(const char *s)
{
    nmea_result_t r = NMEA_NONE, last = NMEA_NONE;
    for (const char *c = s; *c; c++) if ((r = nmea_feed(&P, &F, *c)) != NMEA_NONE) last = r;
    return last;
}
static int near(double a, double b, double eps) { return fabs(a - b) < eps; }

static void t_checksum_known(void)
{
    /* well-known example sentence from the NMEA literature */
    CHECK(nmea_checksum("GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W") == 0x6A);
    CHECK(nmea_checksum("PCAS02,100") == 0x1E);                    /* CASIC spec */
    CHECK(nmea_checksum("PCAS01,1") == 0x1D);
}

static void t_rmc_fix(void)
{
    CHECK(feed_raw("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n") == NMEA_RMC);
    CHECK(F.rmc_fix);
    CHECK(near(F.lat_deg, 48.0 + 7.038 / 60.0, 1e-6));
    CHECK(near(F.lon_deg, 11.0 + 31.0 / 60.0, 1e-6));
    CHECK(near(F.speed_kmh, 22.4 * 1.852, 1e-3));
    CHECK(near(F.course_deg, 84.4, 1e-3));
    CHECK(F.hh == 12 && F.mm == 35 && F.ss == 19);
    CHECK(F.day == 23 && F.month == 3 && F.year == 2094);   /* 2-digit year: 20yy */
}

static void t_rmc_south_west_ms(void)
{
    /* Brisbane-ish, GN talker, NMEA 4.1 mode + nav status fields */
    CHECK(feed_body("GNRMC,021530.250,A,2728.2000,S,15301.2000,W,0.50,,111026,,,A,V") == NMEA_RMC);
    CHECK(F.rmc_fix);
    CHECK(near(F.lat_deg, -(27.0 + 28.2 / 60.0), 1e-6));
    CHECK(near(F.lon_deg, -(153.0 + 1.2 / 60.0), 1e-6));
    CHECK(near(F.speed_kmh, 0.5 * 1.852, 1e-3));
    CHECK(isnan(F.course_deg));                              /* left empty    */
    CHECK(F.ms == 250);
    CHECK(F.day == 11 && F.month == 10 && F.year == 2026);
}

static void t_rmc_void_clears_fix(void)
{
    feed_body("GNRMC,021530.000,A,2728.2000,S,15301.2000,E,10.0,90.0,111026,,,A");
    CHECK(F.rmc_fix);
    CHECK(feed_body("GNRMC,021531.000,V,,,,,,,111026,,,N") == NMEA_RMC);
    CHECK(!F.rmc_fix && isnan(F.lat_deg) && isnan(F.speed_kmh));   /* not 0 km/h */
}

static void t_rmc_mode_n_is_no_fix(void)
{
    CHECK(feed_body("GNRMC,021530.000,A,2728.2000,S,15301.2000,E,10.0,90.0,111026,,,N") == NMEA_RMC);
    CHECK(!F.rmc_fix);
}

static void t_rmc_bad_position_keeps_previous(void)
{
    feed_body("GNRMC,021530.000,A,2728.2000,S,15301.2000,E,10.0,90.0,111026,,,A");
    double lat = F.lat_deg;
    /* status A but the latitude minutes are 75: garbage, not a position */
    CHECK(feed_body("GNRMC,021531.000,A,2775.0000,S,15301.2000,E,99.0,90.0,111026,,,A") == NMEA_OTHER);
    CHECK(F.lat_deg == lat && near(F.speed_kmh, 18.52, 1e-3));      /* untouched */
}

static void t_gga(void)
{
    CHECK(feed_body("GNGGA,021530.000,2728.2000,S,15301.2000,E,1,09,1.10,25.3,M,38.0,M,,") == NMEA_GGA);
    CHECK(F.quality == 1 && F.sats == 9);
    CHECK(near(F.hdop, 1.10, 1e-4) && near(F.alt_m, 25.3, 1e-4));
    CHECK(feed_body("GNGGA,021531.000,,,,,0,00,99.99,,,,,,") == NMEA_GGA);
    CHECK(F.quality == 0 && F.sats == 0 && isnan(F.alt_m));
}

static void t_bad_checksum_rejected(void)
{
    nmea_init(&P, &F);
    CHECK(feed_raw("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B\r\n") == NMEA_BAD);
    CHECK(!F.rmc_fix && isnan(F.lat_deg));
    CHECK(feed_raw("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W\r\n") == NMEA_BAD);
    CHECK(P.bad == 2 && P.ok == 0);
}

static void t_resync_and_overlong(void)
{
    nmea_init(&P, &F);
    /* line cut off mid-way by a new "$" (lost bytes), then garbage, then good */
    feed_raw("$GNRMC,0215");
    feed_raw("\x01\xFF noise ");
    CHECK(feed_body("GNGGA,021530.000,2728.2000,S,15301.2000,E,1,07,1.20,25.0,M,38.0,M,,") == NMEA_GGA);
    CHECK(F.sats == 7);
    char longl[200];
    memset(longl, 'A', sizeof longl);
    longl[0] = '$'; longl[sizeof longl - 1] = '\0';
    CHECK(feed_raw(longl) == NMEA_BAD);                      /* too long: dropped */
    CHECK(feed_body("GNGGA,021531.000,2728.2000,S,15301.2000,E,1,08,1.20,25.0,M,38.0,M,,") == NMEA_GGA);
    CHECK(F.sats == 8);                                      /* and back in sync  */
}

static void t_other_sentence_counts_as_valid(void)
{
    nmea_init(&P, &F);
    CHECK(feed_body("GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30") == NMEA_OTHER);
    CHECK(feed_body("GPTXT,01,01,01,ANTENNA OK") == NMEA_OTHER);
    CHECK(P.ok == 2 && P.bad == 0);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "checksum_known",              t_checksum_known },
        { "rmc_fix",                     t_rmc_fix },
        { "rmc_south_west_ms",           t_rmc_south_west_ms },
        { "rmc_void_clears_fix",         t_rmc_void_clears_fix },
        { "rmc_mode_n_is_no_fix",        t_rmc_mode_n_is_no_fix },
        { "rmc_bad_position_keeps_prev", t_rmc_bad_position_keeps_previous },
        { "gga",                         t_gga },
        { "bad_checksum_rejected",       t_bad_checksum_rejected },
        { "resync_and_overlong",         t_resync_and_overlong },
        { "other_sentence_valid",        t_other_sentence_counts_as_valid },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        nmea_init(&P, &F);
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
