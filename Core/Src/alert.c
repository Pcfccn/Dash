/* alert.c — see alert.h. */
#include "alert.h"
#include "cluster_policy.h"

/* First guesses (alert.h): temperatures move slowly, so a 2 s / 1 s dwell
 * costs nothing and removes threshold flicker; RPM is a redline cue and must
 * not wait; the battery dwell rides out a load step. */
const alert_cfg_t alert_cfg[M_COUNT] = {
/*                used  hyst   upW   upC   down  run */
    [M_RPM]     = { true, 100.0f,   0,    0,  500, false },
    [M_COOL]    = { true,   2.0f, 2000, 1000, 3000, false },
    [M_OIL]     = { true,   3.0f, 2000, 1000, 3000, false },
    [M_ATF]     = { true,   3.0f, 2000, 1000, 3000, false },
    [M_EGT]     = { true,  20.0f, 2000, 1000, 3000, false },
    [M_BATTERY] = { true,   0.2f, 5000, 3000, 3000, true  },
    [M_OILP]    = { true,   0.1f, 1000,  500, 3000, true  },
};

void alert_reset(alert_t *a)
{
    a->shown = ST_OK;
    a->warn_on = a->crit_on = a->below_on = false;
}

metric_state_t alert_step(alert_t *a, const alert_cfg_t *c, metric_key_t k,
                          float v, uint32_t now)
{
    metric_state_t raw = metric_state(k, v);

    /* Up: how long the raw state has held each level without a break. */
    if (raw >= ST_CRIT) { if (!a->crit_on) { a->crit_on = true; a->crit_t = now; } }
    else                a->crit_on = false;
    if (raw >= ST_WARN) { if (!a->warn_on) { a->warn_on = true; a->warn_t = now; } }
    else                a->warn_on = false;
    if (a->shown < ST_CRIT && a->crit_on && now - a->crit_t >= c->up_crit_ms) {
        a->shown = ST_CRIT;
        a->below_on = false;
    } else if (a->shown < ST_WARN && a->warn_on && now - a->warn_t >= c->up_warn_ms) {
        a->shown = ST_WARN;
        a->below_on = false;
    }

    /* Down: the state the reading would have `hyst` further out on either
     * side. For these threshold kinds it is never better than raw, so it
     * being below the shown level means raw is too, with margin. */
    if (a->shown >= ST_WARN) {
        metric_state_t lo = metric_state(k, v - c->hyst);
        metric_state_t hi = metric_state(k, v + c->hyst);
        metric_state_t r  = lo > hi ? lo : hi;
        if (r < a->shown) {
            if (!a->below_on) {
                a->below_on = true;
                a->below_t  = now;
            }
            if (now - a->below_t >= c->down_ms) {
                /* to the level the reading has with that margin: CRIT can
                 * drop to WARN, or straight to OK when it is well inside */
                a->shown    = (r >= ST_WARN) ? r : ST_OK;
                a->below_on = false;
            }
        } else {
            a->below_on = false;
        }
    }
    return a->shown;
}

static alert_t        s_al[M_COUNT];
static metric_state_t s_st[M_COUNT];
static bool           s_run, s_run_cand;
static uint32_t       s_run_t;

void alerts_update(const volatile obd_data_t *d, uint32_t now)
{
    /* Engine running: a fresh RPM above cranking, held for ALERT_RUN_MS. */
    bool turning = policy_shown(d, M_RPM, now) && d->rpm >= ALERT_RUN_RPM;
    if (!turning) {
        s_run_cand = false;
        s_run      = false;
    } else if (!s_run_cand) {
        s_run_cand = true;
        s_run_t    = now;
    } else if (now - s_run_t >= ALERT_RUN_MS) {
        s_run = true;
    }

    for (int i = 0; i < M_COUNT; i++) {
        metric_key_t       k = (metric_key_t)i;
        const alert_cfg_t *c = &alert_cfg[k];
        bool judged = c->used && policy_shown(d, k, now) && (!c->needs_run || s_run);
        if (k == M_OILP && !OILP_VALIDATED) judged = false;
        if (!judged) {
            alert_reset(&s_al[k]);
            s_st[k] = ST_INFO;
            continue;
        }
        s_st[k] = alert_step(&s_al[k], c, k, policy_value(d, k), now);
    }
}

metric_state_t alerts_state(metric_key_t k)
{
    return (k < M_COUNT) ? s_st[k] : ST_INFO;
}

bool alerts_engine_running(void) { return s_run; }
