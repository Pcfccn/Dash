#ifndef ALERT_H
#define ALERT_H

/* The alarm state each judged metric shows, filtered (review C7 SW-05).
 *
 * metric_state() (cluster_config.h) turns one reading into OK / WARN / CRIT.
 * On its own that flickers on a value sitting at a threshold, reds the
 * battery for the second the starter pulls it down, and drops a real alarm
 * the moment one sample dips. Here, per metric (table in alert.c):
 *  - a level is entered only once the raw state has held it for a dwell
 *    (up_crit_ms is the shorter one, so a fast rise goes straight to CRIT);
 *  - it is left only once the reading has been at least `hyst` inside the
 *    better band for down_ms, so a CRIT also stays up at least down_ms;
 *  - battery and oil pressure are judged only with the engine running (RPM
 *    fresh >= ALERT_RUN_RPM for ALERT_RUN_MS): cranking and key-on / engine-
 *    off readings are expected outside their bands; oil pressure also only
 *    once OILP_VALIDATED;
 *  - without a fresh value there is no state ("not judged"), and the filter
 *    starts over when one comes back.
 * The thresholds are still the mockup's, not this engine's calibration, and
 * the dwell / hysteresis values are first guesses: tune both from in-car logs. */

#include <stdbool.h>
#include <stdint.h>
#include "cluster_config.h"
#include "fdcan_obd.h"

#define ALERT_RUN_RPM 500.0f    /* below a warm diesel idle, above cranking */
#define ALERT_RUN_MS  3000u     /* after the start, before judging the rest */

typedef struct {
    bool     used;          /* this metric is filtered here                 */
    float    hyst;          /* how far inside the better band to step down  */
    uint16_t up_warn_ms;    /* raw >= WARN this long -> WARN                */
    uint16_t up_crit_ms;    /* raw >= CRIT this long -> CRIT                */
    uint16_t down_ms;       /* hyst inside this long -> one step down       */
    bool     needs_run;     /* judged only with the engine running          */
} alert_cfg_t;

typedef struct {
    metric_state_t shown;   /* ST_OK, ST_WARN or ST_CRIT                    */
    bool     warn_on, crit_on, below_on;
    uint32_t warn_t, crit_t, below_t;
} alert_t;

extern const alert_cfg_t alert_cfg[M_COUNT];

void           alert_reset(alert_t *a);
/* One fresh, judged reading v of metric k at time now. */
metric_state_t alert_step(alert_t *a, const alert_cfg_t *c, metric_key_t k,
                          float v, uint32_t now);

/* Once per UI refresh, before anything reads alerts_state(). */
void           alerts_update(const volatile obd_data_t *d, uint32_t now);
/* Filtered OK / WARN / CRIT of a used metric; ST_INFO when it is not judged
 * right now (no fresh value, engine not running, not validated). */
metric_state_t alerts_state(metric_key_t k);
bool           alerts_engine_running(void);

#endif /* ALERT_H */
