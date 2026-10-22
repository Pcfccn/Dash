/* Host-side tests for Core/Src/odo.c: GPS / OBD integration, hand-over without
 * double counting, standstill drift, gaps, TRIP auto-reset, civil time. */
#include <stdio.h>
#include <math.h>

#include "../../Core/Src/odo.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static odo_t o;

/* GPS at 10 Hz, constant speed, for ms milliseconds from *t. */
static void gps_run(uint32_t *t, float v, uint32_t ms)
{
    for (uint32_t e = 0; e < ms; e += 100u) { odo_gps(&o, v, true, *t); *t += 100u; }
}

static void obd_run(uint32_t *t, float v, uint32_t ms, uint32_t step)
{
    for (uint32_t e = 0; e < ms; e += step) { odo_obd(&o, v, *t); *t += step; }
}

static void t_gps_constant_speed(void)
{
    odo_init(&o, 1000u, 0u, 0u, 0u, 0u);
    uint32_t t = 5000;
    gps_run(&t, 72.0f, 10100);                 /* 72 km/h = 20 m/s, 10 s of intervals */
    CHECK(NEAR(o.trip_m, 200.0, 0.5) && NEAR(o.total_m, 1200.0, 0.5) && NEAR(o.a_m, 200.0, 0.5));
    CHECK(NEAR(o.trip_s, 10.0, 0.01));
    CHECK(odo_moving(&o, t) && odo_source(&o, t) == ODO_SRC_GPS);
}

static void t_standstill_drift_ignored(void)
{
    odo_init(&o, 0, 0, 0, 0, 0);
    uint32_t t = 0;
    for (int i = 0; i < 600; i++) { odo_gps(&o, (i % 2) ? 1.2f : 0.3f, true, t); t += 100u; }
    CHECK(o.total_m == 0.0 && o.trip_s == 0.0);
    CHECK(!odo_moving(&o, t));
}

static void t_bad_gps_and_gaps(void)
{
    odo_init(&o, 0, 0, 0, 0, 0);
    uint32_t t = 0;
    gps_run(&t, 36.0f, 1100);                  /* 10 m/s, 1.0 s -> 10 m */
    odo_gps(&o, 36.0f, false, t); t += 100u;   /* one bad sample breaks the chain ... */
    gps_run(&t, 36.0f, 1100);                  /* ... the 0.2 s gap is filled: 2 + 10 m */
    CHECK(NEAR(o.total_m, 22.0, 0.1));
    t += 5000u;                                /* 5 s silence: not integrated */
    gps_run(&t, 36.0f, 200);                   /* first sample after it adds nothing */
    CHECK(NEAR(o.total_m, 23.0, 0.1));
}

static void t_obd_when_no_gps(void)
{
    odo_init(&o, 0, 0, 0, 0, 0);
    uint32_t t = 0;
    obd_run(&t, 54.0f, 4100, 100);             /* no GPS ever: 15 m/s, 4 s -> 60 m */
    CHECK(NEAR(o.total_m, 60.0, 0.5));
    CHECK(odo_source(&o, t) == ODO_SRC_OBD);
}

static void t_handover_no_double_count(void)
{
    odo_init(&o, 0, 0, 0, 0, 0);
    uint32_t t = 0;
    /* GPS and OBD both arriving: only GPS counts */
    for (int i = 0; i < 50; i++) {
        odo_gps(&o, 36.0f, true, t);
        odo_obd(&o, 40.0f, t + 30u);
        t += 100u;
    }
    double after_gps = o.total_m;
    CHECK(NEAR(after_gps, 49.0, 0.2));         /* 4.9 s at 10 m/s */
    /* tunnel: GPS stops, OBD carries on; OBD counts only after the last GPS sample */
    uint32_t last_gps = t - 100u;
    for (int i = 0; i < 30; i++) { odo_obd(&o, 36.0f, t + 30u); t += 100u; }
    double tunnel = o.total_m - after_gps;
    double expect = 10.0 * ((t - 100u + 30u) - last_gps) / 1000.0;
    CHECK(NEAR(tunnel, expect, 0.1));      /* incl. the 1.5 s GPS hold window */
    /* out of the tunnel: GPS resumes; its first sample fills the 70 ms since the
     * last OBD sample, then GPS only: 0.7 + 10 m */
    double before = o.total_m;
    gps_run(&t, 36.0f, 1100);
    CHECK(NEAR(o.total_m - before, 10.7, 0.1));
}

static void t_trip_auto_reset(void)
{
    /* parked 2 h: the first GPS time resets TRIP to what was driven since boot */
    odo_init(&o, 50000u, 12000u, 3000u, 900u, 1000000u);
    uint32_t t = 0;
    obd_run(&t, 36.0f, 1100, 100);             /* 10 m before the fix */
    odo_utc(&o, 1000000u + 7200u);
    CHECK(o.trip_auto_reset && NEAR(o.trip_m, 10.0, 0.2) && NEAR(o.a_m, 3010.0, 0.2) && NEAR(o.total_m, 50010.0, 0.2));
    /* decided once: a later time does nothing */
    odo_reset_a(&o);
    odo_utc(&o, 1000000u + 9999u);
    CHECK(NEAR(o.trip_m, 10.0, 0.2) && o.a_m == 0.0);

    /* parked 10 min: same trip */
    odo_init(&o, 50000u, 12000u, 3000u, 900u, 1000000u);
    odo_utc(&o, 1000000u + 600u);
    CHECK(!o.trip_auto_reset && o.trip_m == 12000.0 && o.trip_s == 900.0);

    /* nothing saved with a time before, or a clock that went back: no reset */
    odo_init(&o, 0, 500u, 0, 0, 0);
    odo_utc(&o, 2000000u);
    CHECK(!o.trip_auto_reset && o.trip_m == 500.0);
    odo_init(&o, 0, 500u, 0, 0, 3000000u);
    odo_utc(&o, 2000000u);
    CHECK(!o.trip_auto_reset && o.trip_m == 500.0);
}

static void t_manual_resets(void)
{
    odo_init(&o, 100000u, 2000u, 4000u, 600u, 0);
    odo_reset_trip(&o);
    CHECK(o.trip_m == 0.0 && o.trip_s == 0.0 && o.a_m == 4000.0 && o.total_m == 100000.0);
    odo_reset_a(&o);
    CHECK(o.a_m == 0.0 && o.total_m == 100000.0);
}

static void t_tick_wrap(void)
{
    odo_init(&o, 0, 0, 0, 0, 0);
    uint32_t t = 0xFFFFFFFFu - 450u;
    gps_run(&t, 36.0f, 1100);                  /* crosses 2^32 */
    CHECK(NEAR(o.total_m, 10.0, 0.1));
}

static void t_civil_time(void)
{
    CHECK(odo_utc_from_civil(2000, 1, 1, 0, 0, 0) == 0u);
    CHECK(odo_utc_from_civil(2000, 1, 2, 0, 0, 1) == 86401u);
    CHECK(odo_utc_from_civil(2000, 3, 1, 0, 0, 0) == 60u * 86400u);       /* 2000 is a leap year */
    CHECK(odo_utc_from_civil(2026, 10, 22, 12, 34, 56) == 845987696u);
    CHECK(odo_utc_from_civil(1999, 12, 31, 0, 0, 0) == 0u);
    CHECK(odo_utc_from_civil(2026, 13, 1, 0, 0, 0) == 0u);
}

int main(void)
{
    struct { const char *name; void (*fn)(void); } T[] = {
        { "gps_constant_speed",     t_gps_constant_speed },
        { "standstill_drift",       t_standstill_drift_ignored },
        { "bad_gps_and_gaps",       t_bad_gps_and_gaps },
        { "obd_when_no_gps",        t_obd_when_no_gps },
        { "handover_no_double",     t_handover_no_double_count },
        { "trip_auto_reset",        t_trip_auto_reset },
        { "manual_resets",          t_manual_resets },
        { "tick_wrap",              t_tick_wrap },
        { "civil_time",             t_civil_time },
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = fails;
        T[i].fn();
        printf("%-26s %s\n", T[i].name, fails == f0 ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
