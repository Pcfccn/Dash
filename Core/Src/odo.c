/* odo.c — own odometer, see odo.h. */
#include "odo.h"
#include <math.h>
#include <string.h>

void odo_init(odo_t *o, uint32_t total_m, uint32_t trip_m, uint32_t a_m,
              uint32_t trip_s, uint32_t utc_saved)
{
    memset(o, 0, sizeof *o);
    o->total_m   = total_m;
    o->trip_m    = trip_m;
    o->a_m       = a_m;
    o->trip_s    = trip_s;
    o->utc_saved = utc_saved;
}

static float still0(float v) { return (isnan(v) || v < ODO_STILL_KMH) ? 0.0f : v; }

/* Distance over [t0, t1] at the mean of two speeds (km/h). A standing
 * interval adds nothing but still counts as covered: the next stretch starts
 * at t1, not back where the car stopped (a drift spike 2 s after a stop would
 * otherwise be integrated over the whole 2 s). */
static void add(odo_t *o, float v0, float v1, uint32_t t0, uint32_t t1, odo_src_t src)
{
    uint32_t dt = t1 - t0;
    if (dt == 0u || dt > ODO_MAX_DT_MS) return;
    o->cover   = true;
    o->cover_t = t1;
    double m = ((double)v0 + (double)v1) / 2.0 / 3.6 * ((double)dt / 1000.0);
    if (m <= 0.0) return;
    o->total_m += m;
    o->trip_m  += m;
    o->a_m     += m;
    o->boot_m  += m;
    o->trip_s  += dt / 1000.0;
    o->boot_s  += dt / 1000.0;
    o->src   = src;
    o->src_t = t1;
}

/* t is at most `lim` ms after `ref` — or before it. Receive times of the two
 * sources do not arrive in order: the loop hands the newest GPS fix over
 * before it decodes the CAN frames of the same period, so an OBD speed is
 * often a few ms older than the GPS sample already counted. Unsigned, such a
 * frame would look 49 days late (GPS "absent", the stretch counted twice). */
static bool within(uint32_t t, uint32_t ref, uint32_t lim)
{
    return (int32_t)(t - ref) <= (int32_t)lim;
}

/* Start of the next stretch: where the last one ended when that is recent,
 * else the source's own previous sample. */
static uint32_t from(const odo_t *o, uint32_t own_prev_t, uint32_t t)
{
    if (o->cover && (t - o->cover_t) <= ODO_MAX_DT_MS && (int32_t)(t - o->cover_t) > 0) return o->cover_t;
    return own_prev_t;
}

void odo_gps(odo_t *o, float v, bool good, uint32_t t)
{
    if (!good || isnan(v)) { o->gps_prev = false; return; }
    /* After a gap the old sample says nothing about the speed since: start
     * again like after a bad sample (the gap itself went to OBD, if any). */
    if (o->gps_prev && !within(t, o->gps_prev_t, ODO_MAX_DT_MS)) o->gps_prev = false;
    float vs = still0(v);
    if (o->gps_prev)  add(o, o->gps_prev_v, vs, from(o, o->gps_prev_t, t), t, ODO_SRC_GPS);
    else if (o->cover) add(o, vs, vs, from(o, t, t), t, ODO_SRC_GPS);   /* gap since OBD */
    o->gps_prev   = true;
    o->gps_prev_v = vs;
    o->gps_prev_t = t;
    o->gps_last_t = t;
    o->gps_ever   = true;
}

void odo_obd(odo_t *o, float v, uint32_t t)
{
    if (isnan(v)) { o->obd_prev = false; return; }
    float vs = still0(v);
    bool gps_present = o->gps_ever && within(t, o->gps_last_t, ODO_GPS_HOLD_MS);
    if (!gps_present && o->obd_prev)
        add(o, o->obd_prev_v, vs, from(o, o->obd_prev_t, t), t, ODO_SRC_OBD);
    o->obd_prev   = true;
    o->obd_prev_v = vs;
    o->obd_prev_t = t;
}

void odo_utc(odo_t *o, uint32_t utc)
{
    if (o->utc_checked || utc == 0u) return;
    o->utc_checked = true;
    if (o->utc_saved != 0u && utc > o->utc_saved && utc - o->utc_saved > ODO_TRIP_GAP_S) {
        o->trip_m = o->boot_m;        /* parked long: this power-up is a new trip */
        o->trip_s = o->boot_s;
        o->trip_auto_reset = true;
    }
}

void odo_reset_trip(odo_t *o) { o->trip_m = 0.0; o->trip_s = 0.0; }
void odo_reset_a(odo_t *o)    { o->a_m = 0.0; }

bool odo_moving(const odo_t *o, uint32_t now)
{
    return o->src != ODO_SRC_NONE && within(now, o->src_t, ODO_MAX_DT_MS);
}

odo_src_t odo_source(const odo_t *o, uint32_t now)
{
    if (o->gps_ever && within(now, o->gps_last_t, ODO_GPS_HOLD_MS)) return ODO_SRC_GPS;
    if (o->obd_prev && within(now, o->obd_prev_t, ODO_MAX_DT_MS)) return ODO_SRC_OBD;
    return ODO_SRC_NONE;
}

uint32_t odo_utc_from_civil(uint16_t y, uint8_t mo, uint8_t d, uint8_t hh, uint8_t mm, uint8_t ss)
{
    if (y < 2000u || y > 2135u || mo < 1u || mo > 12u || d < 1u || d > 31u ||
        hh > 23u || mm > 59u || ss > 60u)
        return 0u;
    /* days since 1970-01-01 (H. Hinnant's days_from_civil), then since 2000 */
    int yy = (int)y - (mo <= 2u ? 1 : 0);
    int era = yy / 400;
    unsigned yoe = (unsigned)(yy - era * 400);
    unsigned doy = (153u * (mo + (mo > 2u ? -3 : 9)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    long days = (long)era * 146097L + (long)doe - 719468L - 10957L;
    if (days < 0) return 0u;
    return (uint32_t)days * 86400u + hh * 3600u + mm * 60u + ss;
}
