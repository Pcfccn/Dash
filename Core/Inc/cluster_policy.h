/* ============================================================================
 *  cluster_policy.h
 *  The decisions both displays take from an OBD snapshot — is a value worth
 *  showing, may SNIFF open, what the alert strip summary says — kept apart
 *  from LVGL so the host tests can check them (tests/host/test_fdcan_obd.c).
 *  Header-only: no build-list change for CubeIDE.
 * ========================================================================== */
#ifndef CLUSTER_POLICY_H
#define CLUSTER_POLICY_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "fdcan_obd.h"

/* 1 = SNIFF may open without a fresh selector (bench, another car). In the
 * truck it must be 0: unknown is not "parked". */
#ifndef SNIFF_BENCH_OVERRIDE
#define SNIFF_BENCH_OVERRIDE 0
#endif

static inline float policy_value(const volatile obd_data_t *d, metric_key_t k)
{
    switch (k) {
        case M_SPEED: return d->speed;   case M_RPM:     return d->rpm;
        case M_COOL:  return d->cool;    case M_OIL:     return d->oil;
        case M_ATF:   return d->atf;     case M_EGT:     return d->egt;
        case M_OILP:  return d->oil_press;
        case M_BOOST: return d->boost;   case M_BATTERY: return d->battery;
        case M_IAT:   return d->iat;     case M_LOAD:    return d->load;
        case M_RAIL:  return d->rail;    case M_GEAR:    return (float)d->gear;
        default:      return NAN;
    }
}

/* Decoded at least once and recent enough (metric_stale_ms). */
static inline bool policy_fresh(const volatile obd_data_t *d, metric_key_t k, uint32_t now)
{
    return !isnan(policy_value(d, k)) && obd_is_fresh(d, k, now);
}

/* Worth showing: the link is up and the value is fresh. Otherwise "--". */
static inline bool policy_shown(const volatile obd_data_t *d, metric_key_t k, uint32_t now)
{
    return d->can_ok && policy_fresh(d, k, now);
}

/* The ECM is answering: RPM (0 with the engine off) or coolant is fresh.
 * Broadcasts alone keep can_ok up, so the link says nothing about this. */
static inline bool policy_ecm_fresh(const volatile obd_data_t *d, uint32_t now)
{
    return policy_shown(d, M_RPM, now) || policy_shown(d, M_COOL, now);
}

/* Everything the alert strip judges and this truck supplies is fresh:
 * RPM, coolant, oil temp, ATF, battery and MIL. EGT (not supported by this
 * E98) and oil pressure (not validated) are not required. Without this a
 * single fresh RPM could paint a green NOMINAL over missing temperatures. */
static inline bool policy_complete(const volatile obd_data_t *d, uint32_t now)
{
    static const metric_key_t need[] = { M_RPM, M_COOL, M_OIL, M_ATF, M_BATTERY };
    for (unsigned i = 0; i < sizeof need / sizeof need[0]; i++)
        if (!policy_shown(d, need[i], now)) return false;
    return d->mil_valid;
}

/* SNIFF pauses OBD polling, so it opens (and stays open) only while the
 * truck is known to be parked: a fresh selector in P or N, and no fresh speed
 * above walking pace. Fail closed — CAN lost, a selector never received or
 * gone stale, or R/D: not allowed. The selector broadcast keeps arriving
 * while sniffing (OBD replies do not), so this also ends SNIFF on a shift. */
static inline bool policy_sniff_allowed(const volatile obd_data_t *d, uint32_t now)
{
    if (!d->can_ok) return SNIFF_BENCH_OVERRIDE;
    if (policy_fresh(d, M_SPEED, now) && d->speed > 3.0f) return false;
    if (d->sel_range >= 1 && obd_sel_fresh(d, now))
        return d->sel_range == 1 || d->sel_range == 3;           /* P / N */
    return SNIFF_BENCH_OVERRIDE;
}

/* Alert strip summary, most severe first. NOMINAL needs complete data. */
typedef enum {
    SUM_CAN_LOST = 0,   /* no frames at all                                  */
    SUM_ALARM,          /* a judged value is WARN/CRIT: named on the strip   */
    SUM_NO_ECM,         /* frames, but the ECM answers nothing               */
    SUM_PARTIAL,        /* no alarm among what is known, but not everything is */
    SUM_NOMINAL         /* complete and no alarm                             */
} policy_summary_t;

static inline policy_summary_t policy_summary(const volatile obd_data_t *d, uint32_t now,
                                              bool alarm)
{
    if (!d->can_ok)                return SUM_CAN_LOST;
    if (alarm)                     return SUM_ALARM;
    if (!policy_ecm_fresh(d, now)) return SUM_NO_ECM;
    if (!policy_complete(d, now))  return SUM_PARTIAL;
    return SUM_NOMINAL;
}

#endif /* CLUSTER_POLICY_H */
