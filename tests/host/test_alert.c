/* Host-side tests for Core/Src/alert.c: dwell, hysteresis, the engine-running
 * gate and "not judged" (review C7 SW-05). The thresholds are the real ones
 * from cluster_config.h; only the freshness check is stubbed. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/alert.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* fdcan_obd.c's freshness rule, without the rest of the module. */
bool obd_is_fresh(const volatile obd_data_t *d, metric_key_t k, uint32_t now)
{
    uint16_t lim = metric_stale_ms[k];
    return lim == 0u || (now - d->upd_ms[k]) <= lim;
}

static obd_data_t d;
static uint32_t   now;

static void reset(void)
{
    memset(&d, 0, sizeof d);
    d.speed = d.rpm = d.cool = d.oil = d.iat = d.load = NAN;
    d.boost = d.rail = d.egt = d.battery = d.atf = d.oil_press = NAN;
    d.gear = -1; d.sel_range = -1; d.can_ok = true;
    now = 100000;
    memset(s_al, 0, sizeof s_al);
    s_run = s_run_cand = false;
    alerts_update(&d, now);
}

static void put(metric_key_t k, volatile float *dst, float v) { *dst = v; d.upd_ms[k] = now; }

/* Hold the readings (kept fresh) for ms, in 40 ms UI steps. */
static void run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 40) {
        now += 40;
        for (int k = 0; k < M_COUNT; k++)
            if (!isnan(policy_value(&d, (metric_key_t)k))) d.upd_ms[k] = now;
        alerts_update(&d, now);
    }
}

static void t_warn_needs_dwell(void)
{
    put(M_COOL, &d.cool, 94.0f);
    run(1960);
    CHECK(alerts_state(M_COOL) == ST_OK);
    run(80);
    CHECK(alerts_state(M_COOL) == ST_WARN);
}

static void t_fast_rise_goes_straight_to_crit(void)
{
    put(M_COOL, &d.cool, 99.0f);
    run(960);
    CHECK(alerts_state(M_COOL) == ST_OK);
    run(80);
    CHECK(alerts_state(M_COOL) == ST_CRIT);
}

static void t_flicker_at_threshold_never_warns(void)
{
    for (int i = 0; i < 100; i++) {          /* 92.9 / 93.1 every 120 ms, 12 s */
        put(M_COOL, &d.cool, (i & 1) ? 93.1f : 92.9f);
        run(120);
        CHECK(alerts_state(M_COOL) == ST_OK);
    }
}

static void t_hysteresis_and_steps_down(void)
{
    put(M_COOL, &d.cool, 98.0f);
    run(1200);
    CHECK(alerts_state(M_COOL) == ST_CRIT);
    put(M_COOL, &d.cool, 96.0f);             /* below 97, not 2 inside   */
    run(10000);
    CHECK(alerts_state(M_COOL) == ST_CRIT);
    put(M_COOL, &d.cool, 94.5f);             /* 2 inside CRIT's band     */
    run(2960);
    CHECK(alerts_state(M_COOL) == ST_CRIT);  /* CRIT holds >= down_ms    */
    run(80);
    CHECK(alerts_state(M_COOL) == ST_WARN);  /* one step                 */
    put(M_COOL, &d.cool, 92.0f);             /* below 93, not 2 inside   */
    run(10000);
    CHECK(alerts_state(M_COOL) == ST_WARN);
    put(M_COOL, &d.cool, 90.5f);
    run(3040);
    CHECK(alerts_state(M_COOL) == ST_OK);
}

static void t_dip_does_not_drop_alarm(void)
{
    put(M_OIL, &d.oil, 125.0f);
    run(2100);
    CHECK(alerts_state(M_OIL) == ST_WARN);
    put(M_OIL, &d.oil, 110.0f);              /* one good sample...       */
    run(40);
    put(M_OIL, &d.oil, 125.0f);              /* ...and back              */
    run(3000);
    CHECK(alerts_state(M_OIL) == ST_WARN);
}

static void t_rpm_redline_is_immediate(void)
{
    put(M_RPM, &d.rpm, 3900.0f);
    run(40);
    CHECK(alerts_state(M_RPM) == ST_WARN);
    put(M_RPM, &d.rpm, 3750.0f);             /* within 100 of 3800       */
    run(2000);
    CHECK(alerts_state(M_RPM) == ST_WARN);
    put(M_RPM, &d.rpm, 3650.0f);
    run(520);                                /* first sample + 480 ms    */
    CHECK(alerts_state(M_RPM) == ST_WARN);
    run(40);                                 /* + 500 ms: down           */
    CHECK(alerts_state(M_RPM) == ST_OK);
}

static void t_battery_only_with_engine_running(void)
{
    put(M_BATTERY, &d.battery, 11.0f);       /* key on, engine off       */
    run(20000);
    CHECK(alerts_state(M_BATTERY) == ST_INFO);
    put(M_RPM, &d.rpm, 200.0f);              /* cranking: still not      */
    put(M_BATTERY, &d.battery, 9.5f);
    run(3000);
    CHECK(alerts_state(M_BATTERY) == ST_INFO && !alerts_engine_running());
    put(M_RPM, &d.rpm, 780.0f);              /* started                  */
    put(M_BATTERY, &d.battery, 11.0f);
    run(2960);
    CHECK(!alerts_engine_running() && alerts_state(M_BATTERY) == ST_INFO);
    run(80);
    CHECK(alerts_engine_running());
    run(2960);                               /* judged from "running":   */
    CHECK(alerts_state(M_BATTERY) == ST_OK); /* CRIT dwell not over yet  */
    run(40);
    CHECK(alerts_state(M_BATTERY) == ST_CRIT);
    put(M_RPM, &d.rpm, 0.0f);                /* stalled: not judged      */
    run(40);
    CHECK(alerts_state(M_BATTERY) == ST_INFO && !alerts_engine_running());
}

static void t_battery_window_low_side(void)
{
    put(M_RPM, &d.rpm, 800.0f);
    put(M_BATTERY, &d.battery, 13.8f);
    run(3100);
    CHECK(alerts_state(M_BATTERY) == ST_OK);
    put(M_BATTERY, &d.battery, 11.9f);       /* charging fault           */
    run(5040);
    CHECK(alerts_state(M_BATTERY) == ST_WARN);
    put(M_BATTERY, &d.battery, 12.1f);       /* above 12.0, not 0.2 in   */
    run(6000);
    CHECK(alerts_state(M_BATTERY) == ST_WARN);
    put(M_BATTERY, &d.battery, 12.3f);
    run(3040);
    CHECK(alerts_state(M_BATTERY) == ST_OK);
    put(M_BATTERY, &d.battery, 15.7f);       /* high side too            */
    run(3040);
    CHECK(alerts_state(M_BATTERY) == ST_CRIT);
}

static void t_stale_is_not_judged_and_restarts(void)
{
    put(M_COOL, &d.cool, 99.0f);
    run(1100);
    CHECK(alerts_state(M_COOL) == ST_CRIT);
    now += metric_stale_ms[M_COOL] + 1u;     /* no new reading           */
    alerts_update(&d, now);
    CHECK(alerts_state(M_COOL) == ST_INFO);
    put(M_COOL, &d.cool, 99.0f);             /* back: dwell again        */
    alerts_update(&d, now);
    CHECK(alerts_state(M_COOL) == ST_OK);
    run(1040);
    CHECK(alerts_state(M_COOL) == ST_CRIT);
    d.can_ok = false;                        /* link down                */
    alerts_update(&d, now);
    CHECK(alerts_state(M_COOL) == ST_INFO);
}

/* Never judged while OILP_VALIDATED is 0 (this suite); with 1
 * (test_alert_oilp.c) only once the engine runs: ~0 bar key-on is normal. */
static void t_oil_pressure_gate(void)
{
    put(M_OILP, &d.oil_press, 0.1f);         /* would be CRIT            */
    run(10000);                              /* engine off               */
    CHECK(alerts_state(M_OILP) == ST_INFO);
    put(M_RPM, &d.rpm, 800.0f);
    run(3040 + 560);                         /* running + CRIT dwell     */
#if OILP_VALIDATED
    CHECK(alerts_state(M_OILP) == ST_CRIT);
#else
    CHECK(alerts_state(M_OILP) == ST_INFO);
#endif
}

static void t_unused_metric_not_judged(void)
{
    put(M_IAT, &d.iat, 99.0f);
    run(1000);
    CHECK(alerts_state(M_IAT) == ST_INFO && alerts_state(M_SPEED) == ST_INFO);
}

int main(void)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "warn_needs_dwell",           t_warn_needs_dwell },
        { "fast_rise_straight_to_crit", t_fast_rise_goes_straight_to_crit },
        { "flicker_never_warns",        t_flicker_at_threshold_never_warns },
        { "hysteresis_steps_down",      t_hysteresis_and_steps_down },
        { "dip_does_not_drop_alarm",    t_dip_does_not_drop_alarm },
        { "rpm_redline_immediate",      t_rpm_redline_is_immediate },
        { "battery_needs_engine",       t_battery_only_with_engine_running },
        { "battery_window_low_side",    t_battery_window_low_side },
        { "stale_not_judged_restarts",  t_stale_is_not_judged_and_restarts },
        { "oilp_gate",                  t_oil_pressure_gate },
        { "unused_metric_not_judged",   t_unused_metric_not_judged },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        reset();
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
