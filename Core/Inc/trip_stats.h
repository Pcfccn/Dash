#ifndef TRIP_STATS_H
#define TRIP_STATS_H

/* What the first drives have to measure, kept in RAM since power-up (or the
 * last reset: hold ● on the STATS page). Per signal: how often it really
 * arrives (count, interval histogram -> p95, longest gap) and the range it
 * covered; plus the longest superloop iteration. One photo of the STATS page
 * gives the numbers to set metric_stale_ms by and to hold the alarm
 * thresholds against (review C7 CAN-04 / SW-05).
 *
 * Fed from the decoder's obd_on_stamp() with each frame's receive time, so a
 * display stall does not stretch the intervals. Gaps while SNIFF pauses the
 * poller, or the ECM is off, do land in the histogram: read p95, not max.
 * Not saved anywhere. Pure logic, host-tested (tests/host/test_trip_stats.c). */

#include <stdbool.h>
#include <stdint.h>
#include "fdcan_obd.h"       /* OBD_STAMP_COUNT */

#define TS_CHANNELS OBD_STAMP_COUNT
#define TS_BUCKETS  8u
/* Interval bucket upper edges, ms; the last bucket is everything above. */
extern const uint16_t ts_edge_ms[TS_BUCKETS - 1u];

typedef struct {
    uint32_t n;                  /* updates                                 */
    uint32_t last_ms;            /* receive time of the latest              */
    uint32_t max_gap;            /* longest interval between two, ms        */
    uint32_t hist[TS_BUCKETS];   /* intervals per bucket                    */
    float    vmin, vmax;         /* NaN until the first value               */
} ts_channel_t;

void                trip_stats_reset(uint32_t now);
void                trip_stats_update(uint8_t ch, float v, uint32_t rx_ms);
void                trip_stats_loop(uint32_t loop_ms);    /* each iteration */
const ts_channel_t *trip_stats_channel(uint8_t ch);
/* Upper edge (ms) of the bucket holding the 95th-percentile interval;
 * UINT32_MAX = above the last edge; 0 = fewer than 2 updates. */
uint32_t            trip_stats_p95(uint8_t ch);
uint32_t            trip_stats_since(void);               /* tick of the reset */
uint32_t            trip_stats_loop_max(void);

#endif /* TRIP_STATS_H */
