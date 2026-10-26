/* Host-side tests for Core/Src/perf.c: a simulated standing start at constant
 * acceleration sampled at 10 Hz, interpolation accuracy, arming, aborts,
 * rolling 60-100, best vs last, clear. */
#include <stdio.h>
#include <math.h>

#include "../../Core/Src/perf.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static perf_t p;

static void still(uint32_t *t, uint32_t ms)
{
    for (uint32_t e = 0; e < ms; e += 100u) { perf_gps(&p, 0.2f, true, *t); *t += 100u; }
}

/* Constant acceleration a (m/s^2) from a standstill at time go (ms), sampled
 * every 100 ms from *t until t_end. Sample phase is offset from go on purpose. */
static void accel(uint32_t *t, uint32_t go, double a, uint32_t t_end)
{
    for (; *t <= t_end; *t += 100u) {
        double s = (*t > go) ? (*t - go) / 1000.0 : 0.0;
        perf_gps(&p, (float)(a * s * 3.6), true, *t);
    }
}

static void t_standing_start_accuracy(void)
{
    perf_init(&p);
    uint32_t t = 10000;
    CHECK(p.state == PERF_NO_GPS);
    still(&t, 1500);
    CHECK(p.state == PERF_READY);
    uint32_t go = t - 37u;                     /* the car leaves between two samples */
    double a = 2.8;                            /* m/s^2: 0-100 in 9.92 s */
    accel(&t, go, a, go + 25000u);
    double t100 = (100.0 / 3.6) / a, t60 = (60.0 / 3.6) / a;
    double t8 = sqrt(2.0 * PERF_EIGHTH_M / a), t4 = sqrt(2.0 * PERF_QUARTER_M / a);
    CHECK(NEAR(p.last_s[PF_0_60], t60, 0.03));
    CHECK(NEAR(p.last_s[PF_0_100], t100, 0.03));
    CHECK(NEAR(p.last_s[PF_EIGHTH], t8, 0.05));
    CHECK(NEAR(p.last_s[PF_QUARTER], t4, 0.05));
    CHECK(NEAR(p.trap_kmh[PF_QUARTER], a * t4 * 3.6, 1.0));
    CHECK(NEAR(p.last_s[PF_60_100], t100 - t60, 0.03));      /* rolling, from the same run */
    CHECK(p.best_s[PF_0_100] == p.last_s[PF_0_100]);
}

static void t_needs_ready(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 500);                            /* not long enough */
    CHECK(p.state != PERF_READY);
    accel(&t, t, 3.0, t + 12000u);
    CHECK(isnan(p.last_s[PF_0_100]));          /* no standing start timed */
    CHECK(!isnan(p.last_s[PF_60_100]));        /* rolling one still counts */
}

static void t_lift_ends_run_and_best_kept(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    uint32_t go = t;
    accel(&t, go, 2.0, go + 15000u);           /* 0-100 in ~13.9 s */
    float slow = p.last_s[PF_0_100];
    /* brake to a stop, wait, a faster run */
    for (float v = 108.0f; v > 0.0f; v -= 6.0f) { perf_gps(&p, v, true, t); t += 100u; }
    CHECK(p.state != PERF_RUN);
    still(&t, 1500);
    go = t;
    accel(&t, go, 3.0, go + 12000u);
    CHECK(p.last_s[PF_0_100] < slow && p.best_s[PF_0_100] == p.last_s[PF_0_100]);
    /* and a slower one after: best stays */
    for (float v = 129.0f; v > 0.0f; v -= 6.0f) { perf_gps(&p, v, true, t); t += 100u; }
    still(&t, 1500);
    go = t;
    accel(&t, go, 1.5, go + 22000u);
    CHECK(p.last_s[PF_0_100] > p.best_s[PF_0_100]);
}

static void t_gps_loss_discards(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    uint32_t go = t;
    accel(&t, go, 3.0, go + 3000u);            /* reaches ~32 km/h */
    perf_gps(&p, 40.0f, false, t);             /* fix lost */
    CHECK(p.state == PERF_NO_GPS);
    t += 100u;
    accel(&t, go, 3.0, go + 12000u);           /* comes back mid-run */
    CHECK(isnan(p.last_s[PF_0_100]));          /* that run is not timed */
}

static void t_sample_gap_ends_run(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    uint32_t go = t;
    accel(&t, go, 3.0, go + 2000u);
    t += 1000u;                                /* 1 s without samples */
    accel(&t, go, 3.0, go + 12000u);
    CHECK(isnan(p.last_s[PF_0_100]));
}

/* Review C8: READY, then no good samples for 20 s (a tunnel, a car park),
 * then the fix returns at 55 km/h and rising. That was taken as a start
 * extrapolated back from 55 -> 56 km/h: a made-up 0-60 = 6.00 s and 0-100 =
 * 10.00 s, as LAST and BEST. A gap disarms READY. */
static void t_gap_disarms_ready(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    CHECK(p.state == PERF_READY);
    t += 20000u;
    for (float v = 55.0f; v < 110.0f; v += 1.0f) { perf_gps(&p, v, true, t); t += 100u; }
    CHECK(isnan(p.last_s[PF_0_60]) && isnan(p.best_s[PF_0_60]));
    CHECK(isnan(p.last_s[PF_0_100]) && isnan(p.best_s[PF_0_100]));
    CHECK(p.state != PERF_RUN);
}

/* No sample at all (NMEA stopped, the receiver keeps its fix): perf_tick()
 * ends READY and RUN, so neither "READY - GO" nor a running clock stays. */
static void t_tick_without_samples(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    CHECK(p.state == PERF_READY);
    perf_tick(&p, p.pt + PERF_GAP_MS);         /* not yet */
    CHECK(p.state == PERF_READY);
    perf_tick(&p, p.pt + PERF_GAP_MS + 1u);
    CHECK(p.state == PERF_NO_GPS);
    still(&t, 1500);
    uint32_t go = t;
    accel(&t, go, 3.0, go + 2000u);
    CHECK(p.state == PERF_RUN);
    perf_tick(&p, p.pt + 60000u);
    CHECK(p.state == PERF_NO_GPS && isnan(perf_elapsed(&p, p.pt + 60000u)));
}

static void t_creep_is_not_a_start(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    CHECK(p.state == PERF_READY);
    for (int i = 0; i < 10; i++) { perf_gps(&p, 2.0f, true, t); t += 100u; }   /* 1 s at 2 km/h */
    CHECK(p.state == PERF_READY);              /* could still be a start on its way */
    for (int i = 0; i < 8; i++) { perf_gps(&p, 2.0f, true, t); t += 100u; }    /* > 1.5 s */
    CHECK(p.state == PERF_MOVING);             /* creeping: disarmed */
    accel(&t, t - 700u, 3.0, t + 12000u);      /* then it goes */
    CHECK(isnan(p.last_s[PF_0_100]));          /* not a standing start */
}

static void t_rolling_dip_cancels(void)
{
    perf_init(&p);
    uint32_t t = 0;
    float v = 50.0f;
    for (; v < 80.0f; v += 2.0f) { perf_gps(&p, v, true, t); t += 100u; }   /* past 60 */
    for (; v > 50.0f; v -= 2.0f) { perf_gps(&p, v, true, t); t += 100u; }  /* dips below 55 */
    for (; v < 110.0f; v += 2.0f) { perf_gps(&p, v, true, t); t += 100u; } /* through 60 and 100 */
    /* timed from the second pass through 60: 40 km/h at 20 km/h/s = 2.0 s */
    CHECK(NEAR(p.last_s[PF_60_100], 2.0, 0.02));
}

static void t_clear(void)
{
    perf_init(&p);
    uint32_t t = 0;
    still(&t, 1500);
    accel(&t, t, 3.0, t + 12000u);
    CHECK(!isnan(p.best_s[PF_0_100]));
    perf_clear(&p);
    for (int i = 0; i < PF_N; i++) CHECK(isnan(p.last_s[i]) && isnan(p.best_s[i]));
}

int main(void)
{
    struct { const char *name; void (*fn)(void); } T[] = {
        { "standing_start_accuracy", t_standing_start_accuracy },
        { "needs_ready",             t_needs_ready },
        { "lift_ends_run_best_kept", t_lift_ends_run_and_best_kept },
        { "gps_loss_discards",       t_gps_loss_discards },
        { "sample_gap_ends_run",     t_sample_gap_ends_run },
        { "gap_disarms_ready",       t_gap_disarms_ready },
        { "tick_without_samples",    t_tick_without_samples },
        { "creep_is_not_a_start",    t_creep_is_not_a_start },
        { "rolling_dip_cancels",     t_rolling_dip_cancels },
        { "clear",                   t_clear },
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = fails;
        T[i].fn();
        printf("%-26s %s\n", T[i].name, fails == f0 ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
