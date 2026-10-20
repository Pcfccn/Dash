/* speed_cal.c — see speed_cal.h. */
#include "speed_cal.h"
#include <math.h>
#include <string.h>

/* Recent GPS samples, to pair an OBD speed with the nearest one in time:
 * 16 at 10 Hz is 1.6 s, longer than a display stall. */
#define SC_GPS_N 16u
typedef struct { uint32_t t; float v; bool ok; } sc_gps_t;
static sc_gps_t s_gps[SC_GPS_N];
static uint8_t  s_gps_head, s_gps_cnt;

/* Recent OBD speeds, for the steadiness test. 64 covers SC_STEADY_MS up to
 * ~30 Hz; if the history does not reach that far back the pair is not taken
 * (stricter, never looser). */
#define SC_OBD_N 64u
typedef struct { uint32_t t; float v; } sc_obd_t;
static sc_obd_t s_obd[SC_OBD_N];
static uint8_t  s_obd_head, s_obd_cnt;

static sc_band_t s_band[SC_BANDS];

void speed_cal_reset(void)
{
    memset(s_band, 0, sizeof s_band);
    s_gps_head = s_gps_cnt = 0;
    s_obd_head = s_obd_cnt = 0;
}

void speed_cal_gps(float kmh, bool fix, uint8_t sats, float hdop, uint32_t t_ms)
{
    bool ok = fix && !isnan(kmh) && sats >= SC_MIN_SATS && (isnan(hdop) || hdop <= SC_MAX_HDOP);
    s_gps[s_gps_head] = (sc_gps_t){ t_ms, kmh, ok };
    s_gps_head = (uint8_t)((s_gps_head + 1u) % SC_GPS_N);
    if (s_gps_cnt < SC_GPS_N) s_gps_cnt++;
}

/* Steady: every OBD speed of the last SC_STEADY_MS within SC_STEADY_KMH of
 * each other, and the history really reaches back that far. */
static bool steady(uint32_t now)
{
    float lo = INFINITY, hi = -INFINITY;
    bool  reaches = false;
    for (uint8_t i = 0; i < s_obd_cnt; i++) {
        const sc_obd_t *o = &s_obd[(s_obd_head + SC_OBD_N - 1u - i) % SC_OBD_N];
        uint32_t age = now - o->t;
        if (age > SC_STEADY_MS) { reaches = true; break; }
        if (o->v < lo) lo = o->v;
        if (o->v > hi) hi = o->v;
    }
    return reaches && (hi - lo) <= SC_STEADY_KMH;
}

void speed_cal_obd(float kmh, uint32_t t_ms)
{
    if (isnan(kmh)) return;
    s_obd[s_obd_head] = (sc_obd_t){ t_ms, kmh };
    s_obd_head = (uint8_t)((s_obd_head + 1u) % SC_OBD_N);
    if (s_obd_cnt < SC_OBD_N) s_obd_cnt++;

    if (kmh < SC_MIN_KMH || !steady(t_ms)) return;
    const sc_gps_t *best = NULL;
    uint32_t best_dt = UINT32_MAX;
    for (uint8_t i = 0; i < s_gps_cnt; i++) {
        const sc_gps_t *g = &s_gps[i];
        uint32_t dt = (g->t > t_ms) ? g->t - t_ms : t_ms - g->t;
        if (dt < best_dt) { best_dt = dt; best = g; }
    }
    if (best == NULL || best_dt > SC_PAIR_MS || !best->ok) return;
    uint8_t b = (kmh < 60.0f) ? 0u : (kmh < 80.0f) ? 1u : (kmh < 100.0f) ? 2u : 3u;
    s_band[b].n++;
    s_band[b].gps += best->v;
    s_band[b].obd += kmh;
}

const sc_band_t *speed_cal_band(uint8_t b)
{
    return (b < SC_BANDS) ? &s_band[b] : NULL;
}

float speed_cal_factor(uint8_t b)
{
    double g = 0.0, o = 0.0;
    for (uint8_t i = 0; i < SC_BANDS; i++) {
        if (b < SC_BANDS && i != b) continue;
        g += s_band[i].gps;
        o += s_band[i].obd;
    }
    return (o > 0.0) ? (float)(g / o) : NAN;
}
