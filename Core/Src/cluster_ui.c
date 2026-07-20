/* ============================================================================
 *  cluster_ui.c — LVGL 9 UI for the Holden Colorado 2.8 (E98) cluster.
 *
 *  Faithful port of the visual reference
 *      reference/diesel_dashboard_320x480_h743_rich.html
 *  to LVGL primitives (containers / labels / bars / arc). Palette, geometry
 *  and typography match the HTML; the SVG line-art icons are represented by
 *  compact text tags (LVGL has no vector-icon set), everything else is 1:1.
 *
 *  Layout is absolute-positioned on a 320x480 panel:
 *      y 0..31    alert strip (always visible)
 *      y 32..479  the active page (DRIVE / DPF / DIAG), 320 x 448
 *
 *  Values come from g_obd (fdcan_obd.c). "no data" (bus quiet, or an enhanced
 *  DID not yet mapped) renders as "--" in grey — never a fake green reading.
 * ========================================================================== */

#include "cluster_ui.h"
#include "cluster_config.h"
#include "fdcan_obd.h"
#include "lvgl.h"
#include <stdbool.h>
#include <math.h>

/* ---- palette (from the HTML :root) --------------------------------------- */
/* Palette brightened for the ILI9488 SPI panel: dark background kept for
 * contrast, but text/borders/accents pushed much brighter so it reads crisp
 * and vivid instead of dim/muddy (the reference tones looked washed here). */
/* This ILI9488 only looks crisp with the small-screen recipe: pure black
 * everywhere, bright near-white text, bright borders (outline cards instead of
 * muddy grey fills), saturated accents. Any dark-grey fill/label goes milky. */
#define C_SCREEN     lv_color_hex(0x000000)
#define C_SURFACE    lv_color_hex(0x000000)   /* pure-black cards — no fog on this panel */
#define C_LINE       lv_color_hex(0x2b3947)   /* subtle borders; vivid values carry contrast */
#define C_LINE_STR   lv_color_hex(0x3c4d61)
#define C_TEXT       lv_color_hex(0xffffff)
#define C_TEXT2      lv_color_hex(0xffffff)
#define C_LABEL      lv_color_hex(0xffffff)   /* labels white — grey washes out here */
#define C_MUTED      lv_color_hex(0xe8edf3)   /* captions near-white (no dim grey) */
#define C_FAINT      lv_color_hex(0xc8d2dc)   /* ticks / off-tags: light, still renders */
#define C_OK         lv_color_hex(0x37d67a)   /* softer template green (2bff77 was too acidic) */
#define C_COLD       lv_color_hex(0x54c8ff)   /* temperature < 50C: engine warming up, light blue */
#define C_INFO       lv_color_hex(0x45d0ff)
#define C_WARN       lv_color_hex(0xffc73f)
#define C_CRIT       lv_color_hex(0xff4350)
#define C_TRACK      lv_color_hex(0x2b3d54)
#define C_ALERTBG    lv_color_hex(0x000000)
#define C_TOPBAR     lv_color_hex(0x000000)

#define DEG "\xC2\xB0"          /* UTF-8 degree sign */

/* Montserrat Bold for the big readouts (generated from the TTF via
 * lv_font_conv) so the numbers look heavy like the design reference. */
LV_FONT_DECLARE(montserrat_bold_24);
LV_FONT_DECLARE(montserrat_bold_28);
LV_FONT_DECLARE(montserrat_bold_48);
LV_FONT_DECLARE(montserrat_bold_72);   /* digits+'-' only: the DRIVE speed hero */

#define F12 &lv_font_montserrat_12
#define F14 &lv_font_montserrat_14
#define F20 &lv_font_montserrat_20
#define F24 &montserrat_bold_24    /* small stat values */
#define F28 &montserrat_bold_28    /* rpm / mini / stat / boost */
#define F48 &montserrat_bold_48    /* gear / grid metric values / soot */
#define F72 &montserrat_bold_72    /* speed — matches the 74px design hero */

/* ---- widget handles we update in refresh() ------------------------------- */
static struct {
    lv_obj_t *page[3];

    /* alert strip */
    lv_obj_t *strip;
    lv_obj_t *tag[9];               /* MIL CLT OIL ATF EGT DPF BAT DTC CAN */
    lv_obj_t *tag_ul[9];            /* per-tag state underline (design signature) */
    lv_obj_t *summary;
    lv_obj_t *count;

    /* DRIVE */
    lv_obj_t *gear_val;
    lv_obj_t *speed_val, *rpm_val;
    lv_obj_t *dm_val[4], *dm_bar[4], *dm_dot[4];   /* cool oil atf egt */
    lv_obj_t *boost_val;

    /* DPF */
    lv_obj_t *soot_arc, *soot_val, *regen_title, *regen_hint;
    lv_obj_t *mc_val[4];            /* egt dp since egr */

    /* DIAG */
    lv_obj_t *mil_text, *dtc_msg, *dtc_ring, *dtc_ic;
    lv_obj_t *st_val[4];            /* batt iat load rail */
    lv_obj_t *did_dbg;              /* raw gear byte + last NRC (DID probing) */
} ui;

static uint8_t s_page = 0;
static volatile bool s_dirty = true;

static const metric_key_t DRIVE_M[4] = { M_COOL, M_OIL, M_ATF, M_EGT };
static const metric_key_t MINI_M[4]  = { M_EGT, M_DPF_DP, M_SINCE_REGEN, M_EGR_T };
static const metric_key_t STAT_M[4]  = { M_BATTERY, M_IAT, M_LOAD, M_RAIL };

/* ------------------------------------------------------------------ helpers */
static int iround(float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); }

/* format v honoring 0/1 decimals, integer-only (nano.specs: no %f) */
static void fmt(char *b, size_t n, float v, uint8_t dec)
{
    if (dec == 0) {
        lv_snprintf(b, n, "%d", iround(v));
    } else {
        int t = iround(v * 10.0f);
        int w = t / 10, f = t % 10; if (f < 0) f = -f;
        lv_snprintf(b, n, "%d.%d", w, f);
    }
}

static lv_color_t state_color(metric_state_t s)
{
    switch (s) {
        case ST_CRIT: return C_CRIT;
        case ST_WARN: return C_WARN;
        case ST_INFO: return C_INFO;
        default:      return C_OK;
    }
}

/* "Do we actually have this number?" — g_obd initialises every float to NaN and
 * only a decoded frame overwrites it, so this covers both the enhanced DIDs that
 * are still unmapped and any standard PID this calibration does not support.
 * Blanket-blacklisting metrics (the old is_enhanced()) hid real data the moment
 * a DID started working. */
static bool has_value(float v) { return !isnan(v); }

/* the °C gauge temperatures — get a "cold" blue tint below 50°C */
static bool is_temp(metric_key_t k)
{
    return k == M_COOL || k == M_OIL || k == M_ATF || k == M_EGT;
}

static float mval(const obd_data_t *d, metric_key_t k)
{
    switch (k) {
        case M_SPEED: return d->speed;   case M_RPM:  return d->rpm;
        case M_COOL:  return d->cool;    case M_OIL:  return d->oil;
        case M_ATF:   return d->atf;     case M_EGT:  return d->egt;
        case M_BOOST: return d->boost;   case M_SOOT: return d->soot;
        case M_DPF_DP:return d->dpf_dp;  case M_SINCE_REGEN: return d->since_regen;
        case M_EGR_T: return d->egr_t;   case M_BATTERY: return d->battery;
        case M_IAT:   return d->iat;     case M_LOAD: return d->load;
        case M_RAIL:  return d->rail;    case M_GEAR: return d->gear;
        default: return 0;
    }
}

static int pct_of(metric_key_t k, float v)
{
    const metric_cfg_t *m = &metrics[k];
    float span = m->scale_max - m->scale_min;
    if (span <= 0) return 0;
    int p = iround((v - m->scale_min) * 100.0f / span);
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

/* strip default theme styling off a structural container */
static void plain(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *mk_label(lv_obj_t *p, const char *txt,
                          const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    return l;
}

/* Big value with a small trailing unit, laid out inline and baseline-aligned
 * (the design's "89°C" / "3.1kPa": the unit hugs the number instead of floating
 * at the card corner). Returns the VALUE label so refresh() can update it; the
 * unit label is static. The row auto-sizes to its content, so a centred row
 * re-centres itself when the value changes width (89 -> 421). */
static lv_obj_t *mk_value(lv_obj_t *p, const lv_font_t *vf, lv_color_t vc,
                          const char *unit, const lv_font_t *uf)
{
    lv_obj_t *row = lv_obj_create(p);
    plain(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    /* main: pack left · cross: bottom-align so the small unit sits on the big
     * number's baseline · pad_column: a hair of space before the unit */
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 2, 0);
    lv_obj_t *v = mk_label(row, "--", vf, vc);
    if (unit && unit[0]) {
        lv_obj_t *u = mk_label(row, unit, uf, C_LABEL);
        lv_obj_set_style_pad_bottom(u, 8, 0);   /* lift the unit toward a superscript */
    }
    return v;
}

/* filled surface card with 1px border + rounded corners */
static lv_obj_t *mk_card(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 11, 0);
    lv_obj_set_style_pad_all(c, 9, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

/* horizontal gauge bar (track + colored indicator) */
static lv_obj_t *mk_bar(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *b = lv_bar_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, h / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, C_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, h / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b, C_INFO, LV_PART_INDICATOR);
    lv_bar_set_range(b, 0, 100);
    lv_bar_set_value(b, 0, LV_ANIM_OFF);
    return b;
}

/* thin colored tick (warn/crit marker over a gauge, or a threshold swatch) */
static lv_obj_t *mk_tick(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c)
{
    lv_obj_t *t = lv_obj_create(p);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_size(t, w, h);
    lv_obj_set_style_radius(t, (w < h ? w : h) / 2, 0);
    lv_obj_set_style_bg_color(t, c, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    return t;
}

static lv_obj_t *mk_dot(lv_obj_t *p, int x, int y)
{
    lv_obj_t *d = lv_obj_create(p);
    lv_obj_set_pos(d, x, y);
    lv_obj_set_size(d, 6, 6);
    lv_obj_set_style_radius(d, 3, 0);
    lv_obj_set_style_bg_color(d, C_OK, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    return d;
}

/* three pager dots at the bottom of a page; active one is wider/brighter */
static void mk_pager(lv_obj_t *page, int active)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_t *d = lv_obj_create(page);
        int wide = (i == active);
        lv_obj_set_size(d, wide ? 18 : 7, 7);
        lv_obj_set_pos(d, 140 - 16 + i * 16, 448 - 16);
        lv_obj_set_style_radius(d, 4, 0);
        lv_obj_set_style_bg_color(d, wide ? C_TEXT2 : lv_color_hex(0x344051), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    }
}

/* calibration eyebrow: vehicle/cal ID on the left, page name on the right.
 * Replaces the old centered topbar. '/' is used as the separator because the
 * Montserrat subset has no middle-dot (U+00B7) glyph. */
static lv_obj_t *mk_eyebrow(lv_obj_t *page, const char *title)
{
    lv_obj_t *tb = lv_obj_create(page);
    lv_obj_set_pos(tb, 0, 0);
    lv_obj_set_size(tb, 320, 28);
    lv_obj_set_style_bg_color(tb, C_TOPBAR, 0);
    lv_obj_set_style_bg_opa(tb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tb, C_LINE, 0);
    lv_obj_set_style_border_width(tb, 1, 0);
    lv_obj_set_style_border_side(tb, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(tb, 0, 0);
    lv_obj_set_style_pad_all(tb, 0, 0);
    lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *id = mk_label(tb, "COLORADO 2.8 / E98 / 500 kbit/s", F12, C_MUTED);
    lv_obj_align(id, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_t *pgn = mk_label(tb, title, F12, C_OK);
    lv_obj_align(pgn, LV_ALIGN_RIGHT_MID, -8, 0);
    return tb;
}

/* ------------------------------------------------------------------ page: DRIVE */
static void build_drive(void)
{
    lv_obj_t *pg = ui.page[0];

    mk_eyebrow(pg, "DRIVE");

    /* hero: gear | speed | (boost / rpm stacked) — matches the design's
     * "working state next to speed" layout. No IAT here (it lives on DIAG),
     * no separate boost strip, no rpm bar. */
    lv_obj_t *hero = lv_obj_create(pg);
    lv_obj_set_pos(hero, 0, 28);
    lv_obj_set_size(hero, 320, 120);
    plain(hero);
    lv_obj_set_style_border_color(hero, C_LINE, 0);
    lv_obj_set_style_border_width(hero, 1, 0);
    lv_obj_set_style_border_side(hero, LV_BORDER_SIDE_BOTTOM, 0);

    /* GEAR card (left column) — big "D6" like the 44px design gear */
    lv_obj_t *gear = mk_card(hero, 8, 8, 78, 104);
    lv_obj_set_style_pad_hor(gear, 4, 0);      /* tighter sides so 48px D6 fits 78px */
    lv_obj_t *gl = mk_label(gear, "GEAR", F12, C_MUTED);
    lv_obj_align(gl, LV_ALIGN_TOP_MID, 0, -2);
    ui.gear_val = mk_label(gear, "--", F48, C_TEXT2);
    lv_label_set_recolor(ui.gear_val, true);   /* range letter white, gear no. green */
    lv_obj_align(ui.gear_val, LV_ALIGN_CENTER, 0, 10);

    /* speed (centre column) — 72px hero number, the design's dominant readout.
     * The digits-only font has a 53px line-height (cap height of the 72px em),
     * which matches the design's ~53px visual digit height. */
    ui.speed_val = mk_label(hero, "--", F72, C_TEXT);
    lv_obj_set_width(ui.speed_val, 134);
    lv_obj_set_style_text_align(ui.speed_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui.speed_val, 86, 22);
    lv_obj_t *su = mk_label(hero, "KM / H", F12, C_LABEL);
    lv_obj_set_width(su, 134);
    lv_obj_set_style_text_align(su, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(su, 86, 82);

    /* work column: BOOST (data-cyan) over RPM */
    lv_obj_t *work = mk_card(hero, 220, 8, 92, 104);
    lv_obj_set_style_pad_all(work, 9, 0);
    lv_obj_t *bl = mk_label(work, "BOOST", F12, C_LABEL);
    lv_obj_set_pos(bl, 0, 0);
    ui.boost_val = mk_label(work, "--", F24, C_INFO);
    lv_obj_set_pos(ui.boost_val, 0, 14);
    lv_obj_t *bu = mk_label(work, "bar", F12, C_LABEL);
    lv_obj_set_pos(bu, 48, 24);           /* unit, inline to the right of value */
    lv_obj_t *rl = mk_label(work, "RPM", F12, C_LABEL);
    lv_obj_set_pos(rl, 0, 46);
    ui.rpm_val = mk_label(work, "--", F24, C_TEXT2);
    lv_obj_set_pos(ui.rpm_val, 0, 60);

    /* 2x2 metric grid on a 1px line background (fills the reclaimed height) */
    lv_obj_t *grid = lv_obj_create(pg);
    lv_obj_set_pos(grid, 0, 148);
    lv_obj_set_size(grid, 320, 276);
    lv_obj_set_style_bg_color(grid, C_LINE, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static const char *NAMES[4] = { "COOLANT", "OIL", "ATF", "EGT" };
    const int ch = 137;                                 /* cell height */
    const int cx[4] = { 0, 160, 0, 160 };
    const int cy[4] = { 0, 0, ch + 1, ch + 1 };
    const int cw[4] = { 159, 160, 159, 160 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *cell = lv_obj_create(grid);
        lv_obj_set_pos(cell, cx[i], cy[i]);
        lv_obj_set_size(cell, cw[i], ch);
        lv_obj_set_style_bg_color(cell, C_SCREEN, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell, 0, 0);
        lv_obj_set_style_radius(cell, 0, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *nm = mk_label(cell, NAMES[i], F12, C_LABEL);
        lv_obj_set_pos(nm, 12, 12);
        ui.dm_dot[i] = mk_dot(cell, cw[i] - 16, 14);

        ui.dm_val[i] = mk_value(cell, F48, C_TEXT, DEG "C", F14);
        lv_obj_align(lv_obj_get_parent(ui.dm_val[i]), LV_ALIGN_TOP_MID, 0, 34);

        ui.dm_bar[i] = mk_bar(cell, 12, ch - 40, cw[i] - 24, 6);

        /* warn/crit tick markers over the gauge, positioned from thresholds */
        const metric_cfg_t *mc = &metrics[DRIVE_M[i]];
        if (mc->kind == THR_HIGH_ONLY) {
            int bw = cw[i] - 24;
            float span = mc->scale_max - mc->scale_min;
            int wx = 12 + (int)((mc->warn_high - mc->scale_min) * bw / span);
            int cxk = 12 + (int)((mc->crit_high - mc->scale_min) * bw / span);
            mk_tick(cell, wx, ch - 43, 2, 12, C_WARN);
            mk_tick(cell, cxk, ch - 43, 2, 12, C_CRIT);
        }

        char lo[8], hi[8];
        fmt(lo, sizeof lo, metrics[DRIVE_M[i]].scale_min, 0);
        fmt(hi, sizeof hi, metrics[DRIVE_M[i]].scale_max, 0);
        lv_obj_t *tl = mk_label(cell, lo, F12, C_FAINT);
        lv_obj_set_pos(tl, 12, ch - 28);
        lv_obj_t *th = mk_label(cell, hi, F12, C_FAINT);
        lv_obj_align(th, LV_ALIGN_TOP_RIGHT, -12, ch - 28);
    }

    mk_pager(pg, 0);
}

/* --------------------------------------------------------------- page: DPF */
static void build_dpf(void)
{
    lv_obj_t *pg = ui.page[1];

    mk_eyebrow(pg, "DPF");

    /* hero: soot ring + regeneration summary */
    lv_obj_t *hero = lv_obj_create(pg);
    lv_obj_set_pos(hero, 0, 28);
    lv_obj_set_size(hero, 320, 176);
    plain(hero);
    lv_obj_set_style_border_color(hero, C_LINE, 0);
    lv_obj_set_style_border_width(hero, 1, 0);
    lv_obj_set_style_border_side(hero, LV_BORDER_SIDE_BOTTOM, 0);

    ui.soot_arc = lv_arc_create(hero);
    lv_obj_set_size(ui.soot_arc, 128, 128);
    lv_obj_set_pos(ui.soot_arc, 12, 22);
    lv_arc_set_rotation(ui.soot_arc, 270);
    lv_arc_set_bg_angles(ui.soot_arc, 0, 360);
    lv_arc_set_range(ui.soot_arc, 0, 100);
    lv_arc_set_value(ui.soot_arc, 0);
    lv_obj_set_style_arc_width(ui.soot_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.soot_arc, C_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.soot_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.soot_arc, C_OK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.soot_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(ui.soot_arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(ui.soot_arc, LV_OBJ_FLAG_CLICKABLE);

    ui.soot_val = mk_value(ui.soot_arc, F48, C_TEXT, "%", F14);
    lv_obj_align(lv_obj_get_parent(ui.soot_val), LV_ALIGN_CENTER, 0, -6);
    lv_obj_t *sl = mk_label(ui.soot_arc, "SOOT", F12, C_MUTED);
    lv_obj_align(sl, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *ml = mk_label(hero, "REGENERATION", F12, C_MUTED);
    lv_obj_set_pos(ml, 150, 34);
    ui.regen_title = mk_label(hero, "Inactive", F20, C_TEXT);
    lv_obj_set_pos(ui.regen_title, 150, 52);
    ui.regen_hint = mk_label(hero, "No regeneration\nrequest from ECM.", F14, C_LABEL);
    lv_obj_set_pos(ui.regen_hint, 150, 84);
    mk_tick(hero, 150, 134, 13, 3, C_WARN);
    lv_obj_t *tn1 = mk_label(hero, "70% warning", F12, C_MUTED);
    lv_obj_set_pos(tn1, 168, 128);
    mk_tick(hero, 150, 152, 13, 3, C_CRIT);
    lv_obj_t *tn2 = mk_label(hero, "85% regen threshold", F12, C_MUTED);
    lv_obj_set_pos(tn2, 168, 146);

    /* 2x2 mini-cards. "dP" keeps ASCII: the Montserrat subset has no Greek
     * delta (U+0394) glyph, so "ΔP" from the design would render as a box. */
    static const char *ML[4] = { "EGT", "dP DPF", "SINCE REGEN", "EGR T" DEG };
    static const char *MU[4] = { DEG "C", "kPa", "km", DEG "C" };
    const int mx[4] = { 10, 165, 10, 165 };
    const int my[4] = { 210, 210, 320, 320 };   /* taller cards fill the lower area */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = mk_card(pg, mx[i], my[i], 145, 100);
        lv_obj_t *nm = mk_label(c, ML[i], F12, C_LABEL);
        lv_obj_set_pos(nm, 0, 0);
        ui.mc_val[i] = mk_value(c, F28, C_TEXT, MU[i], F14);
        lv_obj_set_pos(lv_obj_get_parent(ui.mc_val[i]), 0, 20);
    }

    mk_pager(pg, 1);
}

/* -------------------------------------------------------------- page: DIAG */
static void build_diag(void)
{
    lv_obj_t *pg = ui.page[2];

    mk_eyebrow(pg, "DIAG");

    /* MIL summary row */
    lv_obj_t *mil = mk_card(pg, 10, 38, 300, 42);
    lv_obj_set_style_pad_all(mil, 8, 0);
    lv_obj_t *ml = mk_label(mil, "MIL / CHECK ENGINE", F12, C_MUTED);
    lv_obj_set_pos(ml, 0, 0);
    ui.mil_text = mk_label(mil, "--", F14, C_TEXT2);
    lv_obj_set_pos(ui.mil_text, 0, 16);

    /* DTC message area: circular status badge over a centred message.
     * (LVGL borders are solid-only, so the design's dashed outline is
     * approximated with a solid hairline.) */
    lv_obj_t *box = lv_obj_create(pg);
    lv_obj_set_pos(box, 10, 88);
    lv_obj_set_size(box, 300, 104);
    lv_obj_set_style_bg_color(box, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_50, 0);
    lv_obj_set_style_border_color(box, C_LINE_STR, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    ui.dtc_ring = lv_obj_create(box);
    lv_obj_set_size(ui.dtc_ring, 40, 40);
    lv_obj_align(ui.dtc_ring, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(ui.dtc_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(ui.dtc_ring, 20, 0);
    lv_obj_set_style_border_color(ui.dtc_ring, C_OK, 0);
    lv_obj_set_style_border_width(ui.dtc_ring, 2, 0);
    lv_obj_set_style_pad_all(ui.dtc_ring, 0, 0);
    lv_obj_clear_flag(ui.dtc_ring, LV_OBJ_FLAG_SCROLLABLE);
    ui.dtc_ic = mk_label(ui.dtc_ring, LV_SYMBOL_OK, F20, C_OK);
    lv_obj_center(ui.dtc_ic);

    ui.dtc_msg = mk_label(box, "NO STORED CODES", F14, C_TEXT2);
    lv_obj_set_style_text_align(ui.dtc_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.dtc_msg, LV_ALIGN_BOTTOM_MID, 0, -14);

    /* 2x2 stat cards */
    static const char *SL[4] = { "BATT", "IAT", "LOAD", "RAIL" };
    static const char *SU[4] = { "V", DEG "C", "%", "MPa" };
    const int sx[4] = { 10, 165, 10, 165 };
    const int sy[4] = { 200, 200, 308, 308 };   /* taller cards fill down to the pager */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = mk_card(pg, sx[i], sy[i], 145, 100);
        lv_obj_t *nm = mk_label(c, SL[i], F12, C_MUTED);
        lv_obj_set_pos(nm, 0, 0);
        ui.st_val[i] = mk_value(c, F28, C_TEXT, SU[i], F14);
        lv_obj_set_pos(lv_obj_get_parent(ui.st_val[i]), 0, 20);
    }

    /* Enhanced-DID probe readout, in the gap above the pager. Deliberately
     * terse and dim — it is a workbench aid for pinning down the GM DIDs, not
     * part of the design. GEAR shows the raw byte behind the gear label; NRC
     * shows the last negative response (service/code), which distinguishes
     * "module rejected the identifier" from "nothing answered at all". */
    ui.did_dbg = mk_label(pg, "", F12, C_FAINT);
    lv_obj_set_pos(ui.did_dbg, 10, 412);

    mk_pager(pg, 2);
}

/* --------------------------------------------------------------- alert strip */
static void build_strip(lv_obj_t *scr)
{
    ui.strip = lv_obj_create(scr);
    lv_obj_set_pos(ui.strip, 0, 0);
    lv_obj_set_size(ui.strip, 320, 32);
    lv_obj_set_style_bg_color(ui.strip, C_ALERTBG, 0);
    lv_obj_set_style_bg_opa(ui.strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui.strip, C_LINE, 0);
    lv_obj_set_style_border_width(ui.strip, 1, 0);
    lv_obj_set_style_border_side(ui.strip, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(ui.strip, 0, 0);
    lv_obj_set_style_pad_all(ui.strip, 0, 0);
    lv_obj_clear_flag(ui.strip, LV_OBJ_FLAG_SCROLLABLE);

    static const char *TAG[9] = { "MIL","CLT","OIL","ATF","EGT","DPF","BAT","DTC","CAN" };
    for (int i = 0; i < 9; i++) {
        ui.tag[i] = mk_label(ui.strip, TAG[i], F12, C_FAINT);
        lv_obj_align(ui.tag[i], LV_ALIGN_LEFT_MID, 4 + i * 24, -3);
        /* hairline state underline beneath each tag (coloured in refresh) */
        ui.tag_ul[i] = mk_tick(ui.strip, 4 + i * 24, 25, 18, 2, C_LINE);
    }
    ui.count = mk_label(ui.strip, "", F12, C_CRIT);
    lv_obj_align(ui.count, LV_ALIGN_RIGHT_MID, -8, 0);
    ui.summary = mk_label(ui.strip, "NOMINAL", F12, C_OK);
    lv_obj_align(ui.summary, LV_ALIGN_RIGHT_MID, -26, 0);
}

/* =============================================================== public API */
void cluster_ui_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, C_SCREEN, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        ui.page[i] = lv_obj_create(scr);
        lv_obj_set_pos(ui.page[i], 0, 32);
        lv_obj_set_size(ui.page[i], 320, 448);
        lv_obj_set_style_bg_color(ui.page[i], C_SCREEN, 0);
        lv_obj_set_style_bg_opa(ui.page[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ui.page[i], 0, 0);
        lv_obj_set_style_radius(ui.page[i], 0, 0);
        lv_obj_set_style_pad_all(ui.page[i], 0, 0);
        lv_obj_clear_flag(ui.page[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    build_drive();
    build_dpf();
    build_diag();
    build_strip(scr);

    cluster_ui_set_page(0);
}

void cluster_ui_set_page(uint8_t p)
{
    if (p > 2) p = 0;
    s_page = p;
    for (int i = 0; i < 3; i++) {
        if (i == p) lv_obj_clear_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_dirty = true;
}

void cluster_ui_next_page(void)
{
    cluster_ui_set_page((s_page + 1) % 3);
}

uint8_t cluster_ui_get_page(void)
{
    return s_page;
}

/* set one big value + optional bar + optional dot from a metric */
static void set_metric(lv_obj_t *val, lv_obj_t *bar, lv_obj_t *dot,
                       metric_key_t k, const obd_data_t *d, bool live)
{
    float v  = mval(d, k);
    bool  ok = live && has_value(v);
    if (!ok) {
        lv_label_set_text(val, "--");
        lv_obj_set_style_text_color(val, C_MUTED, 0);
        if (dot) lv_obj_set_style_bg_color(dot, C_FAINT, 0);
        if (bar) {
            lv_bar_set_value(bar, 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar, C_FAINT, LV_PART_INDICATOR);
        }
        return;
    }
    char b[16];
    fmt(b, sizeof b, v, metrics[k].decimals);
    lv_label_set_text(val, b);

    metric_state_t s = metric_state(k, v);
    lv_color_t c = state_color(s);
    /* Cold temperature (engine warming up): light blue instead of green. */
    if (s == ST_OK && is_temp(k) && v < 50.0f) c = C_COLD;
    /* Vivid state colouring, matching the design reference: the value takes
     * its state colour (OK=green, INFO=blue, WARN=amber, CRIT=red). The
     * saturated colour is what carries contrast on this weak panel. */
    lv_obj_set_style_text_color(val, c, 0);
    if (dot) lv_obj_set_style_bg_color(dot, c, 0);
    if (bar) {
        lv_bar_set_value(bar, pct_of(k, v), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, c, LV_PART_INDICATOR);
    }
}

void cluster_ui_refresh(void)
{
    /* snapshot the volatile shared struct */
    obd_data_t d;
    d.speed = g_obd.speed; d.rpm = g_obd.rpm; d.cool = g_obd.cool;
    d.oil = g_obd.oil; d.iat = g_obd.iat; d.load = g_obd.load;
    d.boost = g_obd.boost; d.rail = g_obd.rail; d.egt = g_obd.egt;
    d.battery = g_obd.battery; d.atf = g_obd.atf; d.soot = g_obd.soot;
    d.dpf_dp = g_obd.dpf_dp; d.egr_t = g_obd.egr_t; d.since_regen = g_obd.since_regen;
    d.gear = g_obd.gear;
    d.mil = g_obd.mil; d.dtc_count = g_obd.dtc_count; d.can_ok = g_obd.can_ok;
    d.gear_raw = g_obd.gear_raw;
    d.last_nrc_sid = g_obd.last_nrc_sid; d.last_nrc = g_obd.last_nrc;
    bool live = d.can_ok;

    char b[16];

    /* ----- DRIVE ----- */
    /* GEAR: design shows the drive gear as "D6" (range letter + number, the
     * number in nominal green).
     *
     * DID 0x199A is the ENGAGED GEAR RATIO, not the selector range: the road
     * test walked D1/D2/D3 and the raw byte followed 1/2/3, while P also reads
     * 1. So it cannot express P/R/N at all -- an earlier reading of "raw 1 in
     * park" as "1 = P" was one data point fitting a wrong theory. Showing P/R/N
     * from this byte actively lies (it labelled D2 as "R"). Until a real PRNDL
     * identifier is found the honest display is the gear number alone; the DIAG
     * probe line still exposes the raw byte. */
    if (live && d.gear >= 1) lv_snprintf(b, sizeof b, "D#37d67a %d#", (int)d.gear);
    else                     lv_snprintf(b, sizeof b, "--");
    lv_label_set_text(ui.gear_val, b);

    if (live && has_value(d.speed)) { fmt(b, sizeof b, d.speed, 0); lv_label_set_text(ui.speed_val, b); }
    else        lv_label_set_text(ui.speed_val, "--");
    if (live && has_value(d.rpm)) { fmt(b, sizeof b, d.rpm, 0); lv_label_set_text(ui.rpm_val, b); }
    else        lv_label_set_text(ui.rpm_val, "--");
    {   /* RPM stays white, but warns/reds near the redline */
        metric_state_t rs = (live && has_value(d.rpm)) ? metric_state(M_RPM, d.rpm) : ST_OK;
        lv_obj_set_style_text_color(ui.rpm_val, (rs >= ST_WARN) ? state_color(rs) : C_TEXT2, 0);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.dm_val[i], ui.dm_bar[i], ui.dm_dot[i], DRIVE_M[i], &d, live);
    set_metric(ui.boost_val, NULL, NULL, M_BOOST, &d, live);
    /* MIL and CAN state now live only in the rail (see alert strip below). */

    /* ----- DPF ----- */
    set_metric(ui.soot_val, NULL, NULL, M_SOOT, &d, live);
    {
        bool ok = live && has_value(d.soot);
        int sv = ok ? pct_of(M_SOOT, d.soot) : 0;
        lv_arc_set_value(ui.soot_arc, sv);
        metric_state_t ss = ok ? metric_state(M_SOOT, d.soot) : ST_OK;
        lv_obj_set_style_arc_color(ui.soot_arc, ok ? state_color(ss) : C_FAINT, LV_PART_INDICATOR);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.mc_val[i], NULL, NULL, MINI_M[i], &d, live);

    /* ----- DIAG ----- */
    lv_label_set_text(ui.mil_text, d.mil ? "ON" : "OFF");
    lv_obj_set_style_text_color(ui.mil_text, d.mil ? C_CRIT : C_TEXT2, 0);
    if (d.dtc_count > 0) {
        char mb[40];
        lv_snprintf(mb, sizeof mb, "%u STORED CODE%s",
                    (unsigned)d.dtc_count, d.dtc_count == 1 ? "" : "S");
        lv_label_set_text(ui.dtc_msg, mb);
        lv_obj_set_style_text_color(ui.dtc_msg, C_WARN, 0);
        lv_label_set_text(ui.dtc_ic, "!");
        lv_obj_set_style_text_color(ui.dtc_ic, C_WARN, 0);
        lv_obj_set_style_border_color(ui.dtc_ring, C_WARN, 0);
    } else if (live) {
        lv_label_set_text(ui.dtc_msg, "NO STORED CODES");
        lv_obj_set_style_text_color(ui.dtc_msg, C_OK, 0);
        lv_label_set_text(ui.dtc_ic, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(ui.dtc_ic, C_OK, 0);
        lv_obj_set_style_border_color(ui.dtc_ring, C_OK, 0);
    } else {
        lv_label_set_text(ui.dtc_msg, "NO DATA");
        lv_obj_set_style_text_color(ui.dtc_msg, C_MUTED, 0);
        lv_label_set_text(ui.dtc_ic, "-");
        lv_obj_set_style_text_color(ui.dtc_ic, C_MUTED, 0);
        lv_obj_set_style_border_color(ui.dtc_ring, C_FAINT, 0);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.st_val[i], NULL, NULL, STAT_M[i], &d, live);

    {   /* DID probe line — raw gear byte and last negative response */
        char db[48];
        if (d.last_nrc_sid)
            lv_snprintf(db, sizeof db, "GEAR RAW %02X   NRC %02X/%02X",
                        (unsigned)d.gear_raw,
                        (unsigned)d.last_nrc_sid, (unsigned)d.last_nrc);
        else
            lv_snprintf(db, sizeof db, "GEAR RAW %02X   NRC --",
                        (unsigned)d.gear_raw);
        lv_label_set_text(ui.did_dbg, db);
    }

    /* ----- alert strip -----
     * A tag only judges a value we actually have: metric_state() on NaN would
     * fall through every comparison and report a confident ST_OK, and a PID
     * that reads 0 because it is unsupported must not raise CHECK either. */
    metric_state_t ts[9];
    #define TAG_ST(k, v) ((live && has_value(v)) ? metric_state((k), (v)) : ST_OK)
    ts[0] = d.mil ? ST_CRIT : ST_OK;                        /* MIL */
    ts[1] = TAG_ST(M_COOL, d.cool);                         /* CLT */
    ts[2] = TAG_ST(M_OIL, d.oil);                           /* OIL */
    ts[3] = TAG_ST(M_ATF, d.atf);                           /* ATF */
    ts[4] = TAG_ST(M_EGT, d.egt);                           /* EGT */
    ts[5] = TAG_ST(M_SOOT, d.soot);                         /* DPF */
    ts[6] = TAG_ST(M_BATTERY, d.battery);                   /* BAT */
    ts[7] = d.dtc_count > 0 ? ST_WARN : ST_OK;              /* DTC */
    ts[8] = live ? ST_OK : ST_CRIT;                         /* CAN */
    #undef TAG_ST

    int worst = ST_OK, nalarm = 0;
    for (int i = 0; i < 9; i++) {
        /* MIL (0) and DTC (7) are indicators: grey/dim when nominal, not green.
         * The monitored systems glow green when healthy (design signature). */
        bool indicator = (i == 0 || i == 7);
        lv_color_t tc, ulc;
        if      (ts[i] == ST_CRIT) { tc = C_CRIT; ulc = C_CRIT; }
        else if (ts[i] == ST_WARN) { tc = C_WARN; ulc = C_WARN; }
        else                       { tc  = indicator ? C_FAINT : C_MUTED;
                                     ulc = indicator ? C_LINE  : C_OK; }
        lv_obj_set_style_text_color(ui.tag[i], tc, 0);
        lv_obj_set_style_bg_color(ui.tag_ul[i], ulc, 0);
        if (ts[i] > worst) worst = ts[i];
        if (ts[i] >= ST_WARN) nalarm++;
    }

    const char *txt; lv_color_t col;
    if (!live)                 { txt = LV_SYMBOL_WARNING " CAN LOST"; col = C_CRIT; }
    else if (worst == ST_CRIT) { txt = LV_SYMBOL_WARNING " CHECK";    col = C_CRIT; }
    else if (worst == ST_WARN) { txt = LV_SYMBOL_WARNING " WARN";     col = C_WARN; }
    else                       { txt = "NOMINAL";                    col = C_OK;   }
    lv_label_set_text(ui.summary, txt);
    lv_obj_set_style_text_color(ui.summary, col, 0);

    if (nalarm > 0) { lv_snprintf(b, sizeof b, "%d", nalarm); lv_label_set_text(ui.count, b); }
    else            lv_label_set_text(ui.count, "");

    /* strip background tint by worst state */
    lv_color_t sbg = C_ALERTBG, sbd = C_LINE;
    if (worst == ST_CRIT || !live) { sbg = lv_color_hex(0x1a0a0d); sbd = C_CRIT; }
    else if (worst == ST_WARN)     { sbg = lv_color_hex(0x1a1408); sbd = C_WARN; }
    lv_obj_set_style_bg_color(ui.strip, sbg, 0);
    lv_obj_set_style_border_color(ui.strip, sbd, 0);

    s_dirty = false;
}

/* called by fdcan_obd.c whenever a decoded value changes */
void obd_on_update(void)
{
    s_dirty = true;
}
