/* Host-side tests for Core/Src/speed_cal.c: pairing, steadiness, GPS quality,
 * bands, factors. */
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/speed_cal.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t now;
static bool near(float a, float b) { return fabsf(a - b) < 1e-3f; }

/* Drive for ms: OBD speed every 50 ms (v_obd + slope * t), GPS every 100 ms
 * (ratio * the same speed), GPS offset by gps_dt ms. */
static void drive(uint32_t ms, float v_obd, float slope_kmh_s, float ratio,
                  uint8_t sats, float hdop, bool fix, uint32_t gps_dt)
{
    for (uint32_t t = 0; t < ms; t += 50) {
        now += 50;
        float v = v_obd + slope_kmh_s * (float)t / 1000.0f;
        if (t % 100 == 0) speed_cal_gps(v * ratio, fix, sats, hdop, now + gps_dt);
        speed_cal_obd(v, now);
    }
}

static void t_steady_cruise_pairs(void)
{
    speed_cal_reset(); now = 1000;
    drive(10000, 67.0f, 0.0f, 0.98f, 9, 1.0f, true, 0);
    const sc_band_t *b = speed_cal_band(1);                    /* 60-80             */
    CHECK(b->n > 100);
    CHECK(near(speed_cal_factor(1), 0.98f) && near(speed_cal_factor(SC_BANDS), 0.98f));
    CHECK(speed_cal_band(0)->n == 0 && isnan(speed_cal_factor(0)));
}

static void t_needs_two_seconds_of_history(void)
{
    speed_cal_reset(); now = 1000;
    drive(2000, 67.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);          /* exactly 2 s: not yet */
    CHECK(speed_cal_band(1)->n == 0);
    drive(200, 67.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);
    CHECK(speed_cal_band(1)->n > 0);
}

/* Review C8: after a 3 s pause in OBD speeds the first sample alone used to
 * count as "steady" (the one before the pause made the history "reach back").
 * 80 km/h, pause, then 55 with GPS at 70: no pair. Once 2 s of fresh history
 * exist again, pairs resume. */
static void t_pause_breaks_steadiness(void)
{
    speed_cal_reset(); now = 1000;
    drive(3000, 80.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);
    uint32_t n80 = speed_cal_band(2)->n;
    now += 3000;
    speed_cal_gps(70.0f, true, 9, 0.9f, now);
    speed_cal_obd(55.0f, now + 10u);
    CHECK(speed_cal_band(0)->n == 0);
    drive(2500, 55.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);
    CHECK(speed_cal_band(0)->n > 0 && speed_cal_band(2)->n == n80);
}

static void t_accelerating_is_not_steady(void)
{
    speed_cal_reset(); now = 1000;
    drive(10000, 60.0f, 1.5f, 1.0f, 9, 1.0f, true, 0);         /* +1.5 km/h per s   */
    CHECK(speed_cal_band(1)->n == 0 && speed_cal_band(2)->n == 0);
}

static void t_slow_is_ignored(void)
{
    speed_cal_reset(); now = 1000;
    drive(10000, 35.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);
    for (uint8_t b = 0; b < SC_BANDS; b++) CHECK(speed_cal_band(b)->n == 0);
}

static void t_gps_quality_gate(void)
{
    speed_cal_reset(); now = 1000;
    drive(6000, 85.0f, 0.0f, 1.0f, 5, 1.0f, true, 0);          /* too few sats      */
    drive(6000, 85.0f, 0.0f, 1.0f, 9, 3.5f, true, 0);          /* HDOP too high     */
    drive(6000, 85.0f, 0.0f, 1.0f, 9, 1.0f, false, 0);         /* no fix            */
    CHECK(speed_cal_band(2)->n == 0);
    drive(6000, 85.0f, 0.0f, 1.0f, 9, NAN, true, 0);           /* HDOP unknown: ok  */
    CHECK(speed_cal_band(2)->n > 0);
}

static void t_pairing_window(void)
{
    speed_cal_reset(); now = 1000;
    /* GPS 100 ms at 10 Hz, OBD every 50 ms: shifted 160 ms, the nearest GPS
     * is 60 ms or 40 ms away -> pairs; never more than SC_PAIR_MS. */
    drive(6000, 105.0f, 0.0f, 1.0f, 9, 1.0f, true, 160);
    CHECK(speed_cal_band(3)->n > 0);
    speed_cal_reset();
    /* only old GPS (the receiver stopped): nothing pairs */
    drive(3000, 105.0f, 0.0f, 1.0f, 9, 1.0f, true, 0);
    uint32_t n = speed_cal_band(3)->n;
    for (int i = 0; i < 60; i++) { now += 50; speed_cal_obd(105.0f, now); }
    CHECK(speed_cal_band(3)->n <= n + 2);                      /* within 100 ms only */
}

static void t_bands_and_overall(void)
{
    speed_cal_reset(); now = 1000;
    drive(6000, 45.0f, 0.0f, 1.02f, 9, 1.0f, true, 0);
    drive(6000, 90.0f, 0.0f, 0.99f, 9, 1.0f, true, 0);
    CHECK(near(speed_cal_factor(0), 1.02f) && near(speed_cal_factor(2), 0.99f));
    float all = speed_cal_factor(SC_BANDS);
    CHECK(all > 0.99f && all < 1.02f);                          /* weighted by speed */
    CHECK(speed_cal_band(SC_BANDS) == NULL);
}

static void t_nan_obd_ignored(void)
{
    speed_cal_reset(); now = 1000;
    for (int i = 0; i < 100; i++) { now += 50; speed_cal_obd(NAN, now); }
    CHECK(s_obd_cnt == 0);
}

int main(void)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "steady_cruise_pairs",    t_steady_cruise_pairs },
        { "needs_2s_history",       t_needs_two_seconds_of_history },
        { "pause_breaks_steadiness",t_pause_breaks_steadiness },
        { "accelerating_not_steady",t_accelerating_is_not_steady },
        { "slow_is_ignored",        t_slow_is_ignored },
        { "gps_quality_gate",       t_gps_quality_gate },
        { "pairing_window",         t_pairing_window },
        { "bands_and_overall",      t_bands_and_overall },
        { "nan_obd_ignored",        t_nan_obd_ignored },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
