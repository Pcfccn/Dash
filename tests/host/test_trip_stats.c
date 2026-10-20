/* Host-side tests for Core/Src/trip_stats.c: interval histogram, p95, longest
 * gap, value range, reset. */
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/trip_stats.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void feed(uint8_t ch, int n, uint32_t *t, uint32_t step, float v)
{
    for (int i = 0; i < n; i++) { *t += step; trip_stats_update(ch, v, *t); }
}

static void t_reset_is_empty(void)
{
    trip_stats_reset(1234);
    const ts_channel_t *c = trip_stats_channel(M_RPM);
    CHECK(c->n == 0 && c->max_gap == 0 && isnan(c->vmin) && isnan(c->vmax));
    CHECK(trip_stats_p95(M_RPM) == 0 && trip_stats_since() == 1234 && trip_stats_loop_max() == 0);
    CHECK(trip_stats_channel(TS_CHANNELS) == NULL);
}

static void t_regular_updates(void)
{
    trip_stats_reset(0);
    uint32_t t = 1000;
    trip_stats_update(M_RPM, 800.0f, t);                /* first: no interval   */
    feed(M_RPM, 19, &t, 50, 2000.0f);
    const ts_channel_t *c = trip_stats_channel(M_RPM);
    CHECK(c->n == 20 && c->hist[0] == 19 && c->max_gap == 50);
    CHECK(trip_stats_p95(M_RPM) == 100);
    CHECK(c->vmin == 800.0f && c->vmax == 2000.0f);
}

static void t_p95_picks_the_bucket(void)
{
    trip_stats_reset(0);
    uint32_t t = 0;
    trip_stats_update(M_COOL, 90.0f, t);
    feed(M_COOL, 95, &t, 150, 90.0f);                   /* 95 x <=200            */
    feed(M_COOL, 5, &t, 3000, 90.0f);                   /*  5 x <=5000           */
    CHECK(trip_stats_p95(M_COOL) == 200);               /* exactly 95 %          */
    CHECK(trip_stats_channel(M_COOL)->max_gap == 3000);
    feed(M_COOL, 1, &t, 3000, 90.0f);                   /* 95 of 101 < 95 %      */
    CHECK(trip_stats_p95(M_COOL) == 5000);
}

static void t_long_gap_above_last_edge(void)
{
    trip_stats_reset(0);
    trip_stats_update(M_ATF, 60.0f, 0);
    trip_stats_update(M_ATF, 61.0f, 20000);
    CHECK(trip_stats_channel(M_ATF)->hist[TS_BUCKETS - 1u] == 1);
    CHECK(trip_stats_p95(M_ATF) == UINT32_MAX);
}

static void t_nan_and_bad_channel(void)
{
    trip_stats_reset(0);
    trip_stats_update(M_GEAR, NAN, 10);
    trip_stats_update(M_GEAR, 3.0f, 20);
    const ts_channel_t *c = trip_stats_channel(M_GEAR);
    CHECK(c->n == 2 && c->vmin == 3.0f && c->vmax == 3.0f);   /* counted, range ignores NaN */
    trip_stats_update(TS_CHANNELS, 1.0f, 30);                  /* ignored               */
    trip_stats_update(OBD_STAMP_SEL, 1.0f, 30);
    CHECK(trip_stats_channel(OBD_STAMP_SEL)->n == 1);
}

static void t_tick_wrap(void)
{
    trip_stats_reset(0);
    trip_stats_update(M_SPEED, 50.0f, 0xFFFFFFF0u);
    trip_stats_update(M_SPEED, 50.0f, 0x00000050u);            /* 0x60 = 96 ms later */
    CHECK(trip_stats_channel(M_SPEED)->max_gap == 96 && trip_stats_channel(M_SPEED)->hist[0] == 1);
}

static void t_loop_max(void)
{
    trip_stats_reset(0);
    trip_stats_loop(12); trip_stats_loop(230); trip_stats_loop(40);
    CHECK(trip_stats_loop_max() == 230);
    trip_stats_reset(5);
    CHECK(trip_stats_loop_max() == 0);
}

int main(void)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "reset_is_empty",        t_reset_is_empty },
        { "regular_updates",       t_regular_updates },
        { "p95_picks_bucket",      t_p95_picks_the_bucket },
        { "long_gap_last_bucket",  t_long_gap_above_last_edge },
        { "nan_and_bad_channel",   t_nan_and_bad_channel },
        { "tick_wrap",             t_tick_wrap },
        { "loop_max",              t_loop_max },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
