/* perf.h — acceleration timer from GPS speed (a home-made Dragy).
 *
 * Standing start: with good GPS, stand still (< PERF_STILL_KMH) for
 * PERF_READY_MS, then go. The start is the moment the speed would have left
 * 0, extrapolated back along the first two samples that straddle
 * PERF_GO_KMH; every mark after it (60 / 100 km/h, 1/8 and 1/4 mile) is
 * interpolated linearly between the two samples around it, so a 10 Hz
 * receiver gives a few hundredths, not a whole 0.1 s step. A run ends when the
 * speed drops PERF_LIFT_KMH below its peak (lift / brake), after
 * PERF_RUN_MAX_MS, or — discarded — when good GPS goes missing for more than
 * PERF_GAP_MS. A gap also disarms READY (stand still again to re-arm), and
 * perf_tick() ends READY / RUN when no sample comes at all (NMEA stopped), so
 * neither a "READY - GO" nor a running clock outlives the GPS.
 * Rolling 60-100 and 100-200 are taken from any acceleration, standing start
 * or not, as long as the speed does not dip PERF_ROLL_DIP_KMH below the
 * lower mark in between.
 * No rollout (Dragy's 1 ft): times here are from the real start, so a
 * quarter mile reads ~0.2-0.3 s longer than a drag strip's. Pure logic:
 * tests/host/test_perf.c; docs/future/gps-performance-meter.md. */
#ifndef PERF_H
#define PERF_H

#include <stdbool.h>
#include <stdint.h>

#define PERF_STILL_KMH     1.0f
#define PERF_GO_KMH        3.0f
#define PERF_READY_MS      1000u
#define PERF_CREEP_MS      1500u     /* READY, above still but never reaching GO: disarm */
#define PERF_LIFT_KMH      8.0f
#define PERF_RUN_MAX_MS    60000u
#define PERF_GAP_MS        600u
#define PERF_ROLL_DIP_KMH  5.0f
#define PERF_EIGHTH_M      201.168f
#define PERF_QUARTER_M     402.336f

typedef enum { PF_0_60 = 0, PF_0_100, PF_60_100, PF_100_200, PF_EIGHTH, PF_QUARTER, PF_N } perf_item_t;
typedef enum { PERF_NO_GPS = 0, PERF_MOVING, PERF_READY, PERF_RUN } perf_state_t;

typedef struct {
    float last_s[PF_N], best_s[PF_N];   /* NaN = none yet                         */
    float trap_kmh[PF_N];               /* speed at the 1/8 and 1/4 mile marks    */
    perf_state_t state;
    /* run */
    double   t0;                        /* start, ms (fractional)                 */
    double   dist_m;
    float    vmax;
    uint8_t  got;                       /* bit per item reached in this run       */
    /* previous good sample */
    bool     prev;
    float    pv;
    uint32_t pt;
    uint32_t still_t;                   /* standing still since                   */
    bool     still;
    uint32_t last_still_t;              /* last sample below PERF_STILL_KMH       */
    bool     creep;                     /* READY, moving slowly since creep_t     */
    uint32_t creep_t;
    /* rolling marks */
    double   roll60_t, roll100_t;
    bool     roll60, roll100;
} perf_t;

void perf_init(perf_t *p);
void perf_clear(perf_t *p);             /* forget all results                     */
void perf_gps(perf_t *p, float v_kmh, bool good, uint32_t t_ms);
/* From the loop: no sample for PERF_GAP_MS counts as GPS lost. */
void perf_tick(perf_t *p, uint32_t now);
/* Seconds since the start while a run is on, else NaN. */
float perf_elapsed(const perf_t *p, uint32_t now);

#endif /* PERF_H */
