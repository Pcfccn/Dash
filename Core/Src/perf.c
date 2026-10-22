/* perf.c — acceleration timer from GPS speed, see perf.h. */
#include "perf.h"
#include <math.h>
#include <string.h>

void perf_clear(perf_t *p)
{
    for (int i = 0; i < PF_N; i++) p->last_s[i] = p->best_s[i] = p->trap_kmh[i] = NAN;
}

void perf_init(perf_t *p)
{
    memset(p, 0, sizeof *p);
    perf_clear(p);
    p->state = PERF_NO_GPS;
}

static void result(perf_t *p, perf_item_t k, double secs, float trap)
{
    float s = (float)secs;
    p->last_s[k] = s;
    if (isnan(p->best_s[k]) || s < p->best_s[k]) p->best_s[k] = s;
    p->trap_kmh[k] = trap;
}

/* Time at which a quantity going from a (at ta) to b (at tb) passes x. */
static double cross_t(double ta, double a, double tb, double b, double x)
{
    if (b == a) return tb;
    return ta + (x - a) * (tb - ta) / (b - a);
}

static void rolling(perf_t *p, float v0, float v1, uint32_t t0, uint32_t t1)
{
    if (p->roll60 && v1 < 60.0f - PERF_ROLL_DIP_KMH) p->roll60 = false;
    if (p->roll100 && v1 < 100.0f - PERF_ROLL_DIP_KMH) p->roll100 = false;
    if (v0 < 60.0f && v1 >= 60.0f) { p->roll60 = true; p->roll60_t = cross_t(t0, v0, t1, v1, 60.0); }
    if (v0 < 100.0f && v1 >= 100.0f) {
        double t = cross_t(t0, v0, t1, v1, 100.0);
        if (p->roll60) result(p, PF_60_100, (t - p->roll60_t) / 1000.0, NAN);
        p->roll60 = false;
        p->roll100 = true; p->roll100_t = t;
    }
    if (v0 < 200.0f && v1 >= 200.0f) {
        double t = cross_t(t0, v0, t1, v1, 200.0);
        if (p->roll100) result(p, PF_100_200, (t - p->roll100_t) / 1000.0, NAN);
        p->roll100 = false;
    }
}

static void run_step(perf_t *p, float v0, float v1, uint32_t t0, uint32_t t1)
{
    double d0 = p->dist_m;
    p->dist_m += ((double)v0 + (double)v1) / 2.0 / 3.6 * ((t1 - t0) / 1000.0);
    if (v1 > p->vmax) p->vmax = v1;

    static const float mark_kmh[2] = { 60.0f, 100.0f };
    static const perf_item_t mark_item[2] = { PF_0_60, PF_0_100 };
    for (int i = 0; i < 2; i++) {
        if (!(p->got & (1u << mark_item[i])) && v1 >= mark_kmh[i]) {
            double t = cross_t(t0, v0, t1, v1, mark_kmh[i]);
            result(p, mark_item[i], (t - p->t0) / 1000.0, NAN);
            p->got |= (uint8_t)(1u << mark_item[i]);
        }
    }
    static const float mark_m[2] = { PERF_EIGHTH_M, PERF_QUARTER_M };
    static const perf_item_t dist_item[2] = { PF_EIGHTH, PF_QUARTER };
    for (int i = 0; i < 2; i++) {
        if (!(p->got & (1u << dist_item[i])) && p->dist_m >= mark_m[i]) {
            double t = cross_t(t0, d0, t1, p->dist_m, mark_m[i]);
            float trap = (float)(v0 + (v1 - v0) * (t - t0) / (double)(t1 - t0));
            result(p, dist_item[i], (t - p->t0) / 1000.0, trap);
            p->got |= (uint8_t)(1u << dist_item[i]);
        }
    }
    if (v1 < p->vmax - PERF_LIFT_KMH || (t1 - (uint32_t)p->t0) > PERF_RUN_MAX_MS)
        p->state = PERF_MOVING;       /* run over; results stay */
}

void perf_gps(perf_t *p, float v, bool good, uint32_t t)
{
    if (!good || isnan(v)) {
        p->state = PERF_NO_GPS;       /* a run without GPS is discarded */
        p->prev  = false;
        p->still = false;
        p->roll60 = p->roll100 = false;
        return;
    }
    if (p->prev && (t - p->pt) > PERF_GAP_MS) {      /* samples missing */
        if (p->state == PERF_RUN) p->state = PERF_MOVING;
        p->prev = false;
        p->roll60 = p->roll100 = false;
    }
    if (!p->prev) {
        p->prev = true; p->pv = v; p->pt = t;
        if (p->state == PERF_NO_GPS) p->state = PERF_MOVING;
        p->still = (v < PERF_STILL_KMH);
        p->still_t = t;
        return;
    }

    float v0 = p->pv;
    uint32_t t0 = p->pt;
    rolling(p, v0, v, t0, t);

    switch (p->state) {
    case PERF_RUN:
        run_step(p, v0, v, t0, t);
        break;
    case PERF_READY:
        if (v < PERF_STILL_KMH) {
            p->last_still_t = t;
            p->creep = false;
        } else if (v >= PERF_GO_KMH) {
            /* The start: back to 0 along the last two samples (a real start
             * passes 1-3 km/h within a sample or two), not earlier than one
             * interval before the last still sample. */
            double ts = cross_t(t0, v0, t, v, 0.0);
            double lo = (double)p->last_still_t - (double)(t - t0);
            if (ts < lo) ts = lo;
            if (ts > (double)t) ts = (double)t;
            p->state  = PERF_RUN;
            p->creep  = false;
            p->t0     = ts;
            p->dist_m = 0.0;
            p->vmax   = 0.0f;
            p->got    = 0u;
            run_step(p, 0.0f, v, (uint32_t)ts, t);
        } else {
            /* above still, below GO: a start on its way, or creeping. Creeping
             * that never gets to GO disarms; stop again to re-arm. */
            if (!p->creep) { p->creep = true; p->creep_t = t; }
            if (t - p->creep_t >= PERF_CREEP_MS) {
                p->state = PERF_MOVING;
                p->creep = false;
                p->still = false;
            }
        }
        break;
    case PERF_MOVING:
    case PERF_NO_GPS:
    default:
        if (v < PERF_STILL_KMH) {
            if (!p->still) { p->still = true; p->still_t = t; }
            if (t - p->still_t >= PERF_READY_MS) {
                p->state = PERF_READY;
                p->last_still_t = t;
                p->creep = false;
            }
        } else {
            p->still = false;
        }
        break;
    }
    p->pv = v;
    p->pt = t;
}

float perf_elapsed(const perf_t *p, uint32_t now)
{
    if (p->state != PERF_RUN) return NAN;
    return (float)(((double)now - p->t0) / 1000.0);
}
