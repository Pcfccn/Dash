/* trip_stats.c — see trip_stats.h. */
#include "trip_stats.h"
#include <math.h>
#include <string.h>

const uint16_t ts_edge_ms[TS_BUCKETS - 1u] = { 100, 200, 500, 1000, 2000, 5000, 15000 };

static ts_channel_t s_ch[TS_CHANNELS];
static uint32_t     s_since, s_loop_max;

void trip_stats_reset(uint32_t now)
{
    memset(s_ch, 0, sizeof s_ch);
    for (uint8_t i = 0; i < TS_CHANNELS; i++) s_ch[i].vmin = s_ch[i].vmax = NAN;
    s_since    = now;
    s_loop_max = 0;
}

void trip_stats_update(uint8_t ch, float v, uint32_t rx_ms)
{
    if (ch >= TS_CHANNELS) return;
    ts_channel_t *c = &s_ch[ch];
    if (c->n > 0u) {
        uint32_t gap = rx_ms - c->last_ms;
        uint8_t  b   = 0;
        while (b < TS_BUCKETS - 1u && gap > ts_edge_ms[b]) b++;
        c->hist[b]++;
        if (gap > c->max_gap) c->max_gap = gap;
    }
    c->last_ms = rx_ms;
    c->n++;
    if (!isnan(v)) {
        if (isnan(c->vmin) || v < c->vmin) c->vmin = v;
        if (isnan(c->vmax) || v > c->vmax) c->vmax = v;
    }
}

void trip_stats_loop(uint32_t loop_ms)
{
    if (loop_ms > s_loop_max) s_loop_max = loop_ms;
}

const ts_channel_t *trip_stats_channel(uint8_t ch)
{
    return (ch < TS_CHANNELS) ? &s_ch[ch] : NULL;
}

uint32_t trip_stats_p95(uint8_t ch)
{
    if (ch >= TS_CHANNELS || s_ch[ch].n < 2u) return 0u;
    uint32_t total = s_ch[ch].n - 1u, acc = 0u;
    for (uint8_t b = 0; b < TS_BUCKETS; b++) {
        acc += s_ch[ch].hist[b];
        if ((uint64_t)acc * 100u >= (uint64_t)total * 95u)
            return (b < TS_BUCKETS - 1u) ? ts_edge_ms[b] : UINT32_MAX;
    }
    return UINT32_MAX;
}

uint32_t trip_stats_since(void)    { return s_since; }
uint32_t trip_stats_loop_max(void) { return s_loop_max; }
