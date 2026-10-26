/* trip_app.c — TRIP / PERF glue, see trip_app.h. */
#include "trip_app.h"
#include "main.h"            /* HAL_GetTick */
#include <math.h>

/* Odometer and acceleration timer gates. RMC alone has no quality fields, so
 * the last GGA (sats, HDOP) is used with it. The timer wants more than the
 * odometer: a few metres of error in one sample is a whole 0.1 s there. */
#define ODO_MIN_SATS   5u
#define ODO_MAX_HDOP   3.0f
#define PERF_MIN_SATS  6u
#define PERF_MAX_HDOP  2.0f

static odo_t      s_odo;
static perf_t     s_perf;
static trip_sel_t s_sel;
static uint8_t    s_bl_day, s_bl_night;
static bool       s_bl_known;
static bool       s_bl_dirty;
static uint32_t   s_bl_t;
static bool       s_save_now;
static uint32_t   s_last_save;
static bool       s_was_moving;
static uint32_t   s_utc, s_utc_tick;      /* last GPS time and when it came    */
static float      s_speed = NAN;          /* newest speed from either source   */
static uint32_t   s_speed_t;

void trip_app_init(void)
{
    nvlog_init();
    const nv_data_t *d = nvlog_data();
    odo_init(&s_odo, d->odo_total_m, d->odo_trip_m, d->odo_a_m, d->trip_s, d->utc);
    perf_init(&s_perf);
    s_bl_known = d->bl_day != 0u && d->bl_night != 0u;
    s_bl_day   = d->bl_day;
    s_bl_night = d->bl_night;
}

bool trip_app_brightness(uint8_t *day, uint8_t *night)
{
    if (!s_bl_known) return false;
    *day = s_bl_day; *night = s_bl_night;
    return true;
}

void trip_app_set_brightness(uint8_t day, uint8_t night, uint32_t now)
{
    if (s_bl_known && day == s_bl_day && night == s_bl_night) return;
    s_bl_day = day; s_bl_night = night; s_bl_known = true;
    s_bl_dirty = true;
    s_bl_t = now;
}

/* The GGA must be current too: its quality, sats and HDOP stay in the fix
 * after GGAs stop coming (gps_gga_fresh). */
static bool gps_good(const gps_state_t *g, uint8_t min_sats, float max_hdop)
{
    return g->fix.rmc_fix && gps_gga_fresh(g->rmc_ms) && g->fix.quality >= 1u &&
           g->fix.sats >= min_sats &&
           (isnan(g->fix.hdop) || g->fix.hdop <= max_hdop) && !isnan(g->fix.speed_kmh);
}

void trip_app_gps(const gps_state_t *g)
{
    uint32_t t = g->rmc_ms;          /* this RMC; with a fix, also fix_ms */
    odo_gps(&s_odo, g->fix.speed_kmh, gps_good(g, ODO_MIN_SATS, ODO_MAX_HDOP), t);
    perf_gps(&s_perf, g->fix.speed_kmh, gps_good(g, PERF_MIN_SATS, PERF_MAX_HDOP), t);
    if (g->fix.rmc_fix && !isnan(g->fix.speed_kmh)) { s_speed = g->fix.speed_kmh; s_speed_t = t; }
    uint32_t utc = odo_utc_from_civil(g->fix.year, g->fix.month, g->fix.day,
                                      g->fix.hh, g->fix.mm, g->fix.ss);
    if (g->fix.rmc_fix && utc != 0u && g->fix.year >= 2024u) {
        s_utc = utc; s_utc_tick = t;
        odo_utc(&s_odo, utc);
    }
}

void trip_app_obd_speed(float v, uint32_t rx_ms)
{
    odo_obd(&s_odo, v, rx_ms);
    if (!isnan(v) && odo_source(&s_odo, rx_ms) != ODO_SRC_GPS) { s_speed = v; s_speed_t = rx_ms; }
}

bool trip_app_standing(uint32_t now)
{
    return !isnan(s_speed) && (now - s_speed_t) <= ODO_MAX_DT_MS && s_speed < TRIP_RESET_KMH;
}

static void save(uint32_t now)
{
    nv_data_t d;
    d.bl_day      = s_bl_known ? s_bl_day : 0u;
    d.bl_night    = s_bl_known ? s_bl_night : 0u;
    d.odo_total_m = (uint32_t)s_odo.total_m;
    d.odo_trip_m  = (uint32_t)s_odo.trip_m;
    d.odo_a_m     = (uint32_t)s_odo.a_m;
    d.trip_s      = (uint32_t)s_odo.trip_s;
    /* GPS time now (extrapolated from the last fix), else the last one saved:
     * parking time is measured from whatever was known last. */
    d.utc = s_utc ? s_utc + (now - s_utc_tick) / 1000u : nvlog_data()->utc;
    nvlog_save(&d);
    s_last_save = now;
    s_bl_dirty  = false;              /* any record carries the brightness */
}

void trip_app_service(uint32_t now)
{
    perf_tick(&s_perf, now);          /* no NMEA at all: no READY, no running clock */
    bool moving = odo_moving(&s_odo, now);
    if (s_save_now) {
        s_save_now = false;
        save(now);
    } else if (moving && now - s_last_save >= TRIP_SAVE_MS) {
        save(now);
    } else if (s_was_moving && !moving) {
        save(now);                    /* stopped: the last metres go in now */
    } else if (s_bl_dirty && now - s_bl_t >= TRIP_BL_SAVE_MS) {
        save(now);
    }
    s_was_moving = moving;
    nvlog_service();
}

void trip_app_select_next(void) { s_sel = (trip_sel_t)((s_sel + 1u) % TRIP_SEL_N); }

bool trip_app_reset_selected(uint32_t now)
{
    if (!trip_app_standing(now)) return false;
    if (s_sel == TRIP_SEL_TRIP)   odo_reset_trip(&s_odo);
    else if (s_sel == TRIP_SEL_A) odo_reset_a(&s_odo);
    else                          return false;   /* TOTAL: never by hand */
    s_save_now = true;
    return true;
}

bool trip_app_perf_clear(uint32_t now)
{
    if (!trip_app_standing(now)) return false;
    perf_clear(&s_perf);
    return true;
}

const odo_t  *trip_app_odo(void)      { return &s_odo; }
const perf_t *trip_app_perf(void)     { return &s_perf; }
trip_sel_t    trip_app_selected(void) { return s_sel; }
