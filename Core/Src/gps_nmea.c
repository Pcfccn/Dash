/* NMEA 0183 parser — see gps_nmea.h. */

#include "gps_nmea.h"
#include <math.h>
#include <string.h>

void nmea_init(nmea_parser_t *p, nmea_fix_t *fix)
{
    memset(p, 0, sizeof *p);
    memset(fix, 0, sizeof *fix);
    fix->speed_kmh = NAN; fix->course_deg = NAN;
    fix->lat_deg = NAN;   fix->lon_deg = NAN;
    fix->hdop = NAN;      fix->alt_m = NAN;
}

uint8_t nmea_checksum(const char *body)
{
    uint8_t cs = 0;
    while (*body) cs ^= (uint8_t)*body++;
    return cs;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* ---- fields: [s, s + n) of the sentence, never NUL-terminated ---------- */
typedef struct { const char *s; uint8_t n; } fld_t;

/* Plain decimal "123.45" / "-3.2". Empty or anything else: false. Small and
 * exact enough for NMEA; strtod would pull in far more code than it saves. */
static bool num(fld_t f, double *out)
{
    if (f.n == 0u) return false;
    uint8_t i = 0;
    bool neg = false, digits = false;
    if (f.s[0] == '-') { neg = true; i = 1; }
    double v = 0.0, scale = 0.0;
    for (; i < f.n; i++) {
        char c = f.s[i];
        if (c == '.') { if (scale != 0.0) return false; scale = 1.0; continue; }
        if (c < '0' || c > '9') return false;
        digits = true;
        if (scale == 0.0) v = v * 10.0 + (c - '0');
        else { scale *= 0.1; v += (c - '0') * scale; }
    }
    if (!digits) return false;
    *out = neg ? -v : v;
    return true;
}

/* NMEA angle "ddmm.mmmm" / "dddmm.mmmm" plus hemisphere -> signed degrees. */
static bool angle(fld_t v, fld_t hemi, char pos, char neg, double max, double *out)
{
    double x;
    if (!num(v, &x) || x < 0.0 || hemi.n != 1u) return false;
    double deg = floor(x / 100.0);
    double min = x - deg * 100.0;
    if (min >= 60.0) return false;
    deg += min / 60.0;
    if (deg > max) return false;
    if (hemi.s[0] == neg) deg = -deg;
    else if (hemi.s[0] != pos) return false;
    *out = deg;
    return true;
}

/* Two decimal digits at f.s[k]. */
static bool dd(fld_t f, uint8_t k, uint8_t *out)
{
    if (k + 2u > f.n) return false;
    char a = f.s[k], b = f.s[k + 1u];
    if (a < '0' || a > '9' || b < '0' || b > '9') return false;
    *out = (uint8_t)((a - '0') * 10 + (b - '0'));
    return true;
}

#define MAX_FIELDS 20u

/* RMC: time, status, lat, N/S, lon, E/W, speed kn, course, date, magvar,
 * E/W, mode (NMEA 2.3+), nav status (4.1). */
static bool parse_rmc(const fld_t *f, uint8_t nf, nmea_fix_t *x)
{
    if (nf < 10u) return false;
    bool fix = f[2].n == 1u && f[2].s[0] == 'A';
    if (nf > 12u && f[12].n == 1u && f[12].s[0] == 'N') fix = false;

    double lat = NAN, lon = NAN, kn = NAN, crs = NAN;
    if (fix && (!angle(f[3], f[4], 'N', 'S', 90.0, &lat) ||
                !angle(f[5], f[6], 'E', 'W', 180.0, &lon)))
        return false;                       /* status A but no usable position */
    bool have_kn = num(f[7], &kn) && kn >= 0.0;
    bool have_crs = num(f[8], &crs) && crs >= 0.0 && crs < 360.0;

    uint8_t hh, mm, ss, day, mon, yy;
    bool have_t = dd(f[1], 0, &hh) && dd(f[1], 2, &mm) && dd(f[1], 4, &ss) && hh < 24u && mm < 60u && ss < 61u;
    bool have_d = f[9].n == 6u && dd(f[9], 0, &day) && dd(f[9], 2, &mon) && dd(f[9], 4, &yy) &&
                  day >= 1u && day <= 31u && mon >= 1u && mon <= 12u;

    x->rmc_fix = fix;
    x->lat_deg = fix ? lat : NAN;
    x->lon_deg = fix ? lon : NAN;
    x->speed_kmh  = (fix && have_kn)  ? (float)(kn * 1.852) : NAN;
    x->course_deg = (fix && have_crs) ? (float)crs : NAN;
    if (have_t) {
        x->hh = hh; x->mm = mm; x->ss = ss;
        double t;
        x->ms = (num(f[1], &t)) ? (uint16_t)((t - floor(t)) * 1000.0 + 0.5) : 0u;
        if (x->ms > 999u) x->ms = 999u;
    }
    if (have_d) { x->day = day; x->month = mon; x->year = (uint16_t)(2000u + yy); }
    return true;
}

/* GGA: time, lat, N/S, lon, E/W, quality, sats, HDOP, altitude, M, ... */
static bool parse_gga(const fld_t *f, uint8_t nf, nmea_fix_t *x)
{
    if (nf < 10u) return false;
    double q, sv, h, alt;
    if (!num(f[6], &q) || q < 0.0 || q > 9.0) return false;
    x->quality = (uint8_t)q;
    x->sats = (num(f[7], &sv) && sv >= 0.0 && sv < 100.0) ? (uint8_t)sv : 0u;
    x->hdop = (num(f[8], &h) && h >= 0.0) ? (float)h : NAN;
    x->alt_m = (x->quality != 0u && num(f[9], &alt)) ? (float)alt : NAN;
    return true;
}

/* A whole line "$....*hh" is in p->buf (without CR LF). */
static nmea_result_t sentence(nmea_parser_t *p, nmea_fix_t *x)
{
    char *b = p->buf;
    uint8_t n = p->len;
    if (n < 9u || b[0] != '$' || b[n - 3u] != '*') { p->bad++; return NMEA_BAD; }
    int h1 = hexval(b[n - 2u]), h2 = hexval(b[n - 1u]);
    uint8_t cs = 0;
    for (uint8_t i = 1; i < n - 3u; i++) cs ^= (uint8_t)b[i];
    if (h1 < 0 || h2 < 0 || cs != (uint8_t)(h1 * 16 + h2)) { p->bad++; return NMEA_BAD; }
    p->ok++;

    fld_t f[MAX_FIELDS];
    uint8_t nf = 0;
    uint8_t start = 1;
    for (uint8_t i = 1; i <= n - 3u && nf < MAX_FIELDS; i++) {
        if (i == n - 3u || b[i] == ',') {
            f[nf].s = &b[start];
            f[nf].n = (uint8_t)(i - start);
            nf++;
            start = (uint8_t)(i + 1u);
        }
    }
    /* f[0] = talker + type, e.g. "GNRMC" */
    if (f[0].n == 5u && memcmp(f[0].s + 2, "RMC", 3) == 0)
        return parse_rmc(f, nf, x) ? NMEA_RMC : NMEA_OTHER;
    if (f[0].n == 5u && memcmp(f[0].s + 2, "GGA", 3) == 0)
        return parse_gga(f, nf, x) ? NMEA_GGA : NMEA_OTHER;
    return NMEA_OTHER;
}

nmea_result_t nmea_feed(nmea_parser_t *p, nmea_fix_t *fix, char c)
{
    if (c == '$') {                          /* start (or restart mid-line)    */
        if (p->in) p->bad++;                 /* the previous one never ended   */
        p->in = true;
        p->len = 0;
        p->buf[p->len++] = c;
        return NMEA_NONE;
    }
    if (!p->in) return NMEA_NONE;
    if (c == '\r' || c == '\n') {
        p->in = false;
        return sentence(p, fix);
    }
    if (p->len >= NMEA_LINE_MAX - 1u || (uint8_t)c < 0x20u || (uint8_t)c > 0x7Eu) {
        p->in = false;                       /* too long or not text: drop it  */
        p->bad++;
        return NMEA_BAD;
    }
    p->buf[p->len++] = c;
    return NMEA_NONE;
}
