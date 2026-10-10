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
#include "can_sniff.h"
#include "fault.h"
#include "app_main.h"         /* app_loop_max_ms() for the DIAG readout */
#include "lv_port_disp.h"     /* SPI error counters for the DIAG readout */
#include "st7735_status.h"
#include "lvgl.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>

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
#define C_ORANGE     lv_color_hex(0xff7a1a)   /* reverse gear: bright orange */
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

    /* full-screen alert border overlay (red = critical, orange = reverse) */
    lv_obj_t *border;

    /* alert strip */
    lv_obj_t *strip;
    lv_obj_t *tag[8];               /* MIL CLT OIL ATF EGT BAT DTC CAN */
    lv_obj_t *tag_ul[8];            /* per-tag state underline (design signature) */
    lv_obj_t *summary;
    lv_obj_t *count;

    /* DRIVE */
    lv_obj_t *gear_val;
    lv_obj_t *speed_val, *rpm_val;
    lv_obj_t *dm_val[4], *dm_bar[4], *dm_dot[4];   /* cool oil atf egt */
    lv_obj_t *boost_val;

    /* DIAG */
    lv_obj_t *mil_text, *dtc_msg, *dtc_ring, *dtc_ic;
    lv_obj_t *st_val[4];            /* batt iat load rail */
    lv_obj_t *did_dbg;              /* raw gear byte + last NRC (DID probing) */

    /* SNIFF */
    lv_obj_t *sn_stat, *sn_rows, *sn_ana;
} ui;

/* SNIFF is a workbench page for reverse-engineering unknown CAN IDs.
 * It sits last so DRIVE and DIAG keep their natural order. */
#define PAGE_SNIFF  2
#define PAGE_COUNT  3

static uint8_t s_page = 0;

static const metric_key_t DRIVE_M[4] = { M_COOL, M_OIL, M_ATF, M_OILP };
static const metric_key_t STAT_M[4]  = { M_BATTERY, M_IAT, M_LOAD, M_RAIL };

/* ------------------------------------------------------------------ helpers */
static int iround(float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); }

/* format v honoring 0/1 decimals, integer-only (nano.specs: no %f) */
static void fmt(char *b, size_t n, float v, uint8_t dec)
{
    if (dec == 0) {
        lv_snprintf(b, n, "%d", iround(v));
    } else {
        /* Sign handled apart: -0.4 has a 0 integer part, so "%d.%d" of
         * (t/10, t%10) used to print "0.4". */
        int t = iround(v * 10.0f);
        const char *sign = (t < 0) ? "-" : "";
        if (t < 0) t = -t;
        lv_snprintf(b, n, "%s%d.%d", sign, t / 10, t % 10);
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
        case M_OILP:  return d->oil_press;
        case M_BOOST: return d->boost;   case M_BATTERY: return d->battery;
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

/* pager dots at the bottom of a page; active one is wider/brighter.
 * Kept centred on the same point the three-page layout used, so adding the
 * SNIFF page does not shift the row the eye is used to. */
static void mk_pager(lv_obj_t *page, int active)
{
    const int span = (PAGE_COUNT - 1) * 16 + 7;
    const int x0   = 144 - span / 2;
    for (int i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *d = lv_obj_create(page);
        int wide = (i == active);
        lv_obj_set_size(d, wide ? 18 : 7, 7);
        lv_obj_set_pos(d, x0 + i * 16, 448 - 16);
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
    lv_obj_t *pgn = mk_label(tb, title, F12, C_OK);
    lv_obj_align(pgn, LV_ALIGN_RIGHT_MID, -8, 0);
    return tb;
}

/* ------------------------------------------------------------------ page: DRIVE */
static void build_drive(void)
{
    lv_obj_t *pg = ui.page[0];

    /* No eyebrow on DRIVE: the gear+speed hero already identifies the page and
     * the pager dots show position, so the 28px bar was dead space. The hero and
     * the metric grid start at the very top and use the reclaimed height. */

    /* hero: gear | speed | (boost / rpm stacked) — matches the design's
     * "working state next to speed" layout. No IAT here (it lives on DIAG),
     * no separate boost strip, no rpm bar. */
    lv_obj_t *hero = lv_obj_create(pg);
    lv_obj_set_pos(hero, 0, 0);
    lv_obj_set_size(hero, 320, 128);
    plain(hero);
    lv_obj_set_style_border_color(hero, C_LINE, 0);
    lv_obj_set_style_border_width(hero, 1, 0);
    lv_obj_set_style_border_side(hero, LV_BORDER_SIDE_BOTTOM, 0);

    /* GEAR card (left column) — big "D6" like the 44px design gear */
    lv_obj_t *gear = mk_card(hero, 8, 12, 78, 104);
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
    lv_obj_set_pos(ui.speed_val, 86, 26);
    lv_obj_t *su = mk_label(hero, "KM / H", F12, C_LABEL);
    lv_obj_set_width(su, 134);
    lv_obj_set_style_text_align(su, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(su, 86, 86);

    /* work column: BOOST (data-cyan) over RPM */
    lv_obj_t *work = mk_card(hero, 220, 12, 92, 104);
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

    /* 2x2 metric grid on a 1px line background (fills the reclaimed height:
     * starts right under the hero and runs down to just above the pager) */
    lv_obj_t *grid = lv_obj_create(pg);
    lv_obj_set_pos(grid, 0, 128);
    lv_obj_set_size(grid, 320, 300);
    lv_obj_set_style_bg_color(grid, C_LINE, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static const char *NAMES[4] = { "COOLANT", "OIL", "ATF", "OIL P" };
    static const char *UNITS[4] = { DEG "C",   DEG "C", DEG "C", "bar" };
    const int ch = 149;                                 /* cell height */
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

        ui.dm_val[i] = mk_value(cell, F48, C_TEXT, UNITS[i], F14);
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

/* -------------------------------------------------------------- page: DIAG */
static void build_diag(void)
{
    lv_obj_t *pg = ui.page[1];

    /* Left side of the eyebrow: why the previous run ended. A watchdog or
     * fault reset is amber so it is noticed; a normal power-up stays quiet. */
    lv_obj_t *tb = mk_eyebrow(pg, "DIAG");
    char rb[32];
    lv_snprintf(rb, sizeof rb, "LAST RESET: %s", fault_reset_text());
    lv_obj_t *rst = mk_label(tb, rb, F12, fault_reset_abnormal() ? C_WARN : C_MUTED);
    lv_obj_align(rst, LV_ALIGN_LEFT_MID, 8, 0);

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

    ui.dtc_msg = mk_label(box, "NO DATA", F14, C_MUTED);
    lv_obj_set_style_text_align(ui.dtc_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.dtc_msg, LV_ALIGN_BOTTOM_MID, 0, -14);

    /* 2x2 stat cards */
    static const char *SL[4] = { "BATT", "IAT", "LOAD", "RAIL" };
    static const char *SU[4] = { "V", DEG "C", "%", "bar" };
    const int sx[4] = { 10, 165, 10, 165 };
    const int sy[4] = { 198, 198, 292, 292 };   /* room below for two probe lines */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = mk_card(pg, sx[i], sy[i], 145, 88);
        lv_obj_t *nm = mk_label(c, SL[i], F12, C_MUTED);
        lv_obj_set_pos(nm, 0, 0);
        ui.st_val[i] = mk_value(c, F28, C_TEXT, SU[i], F14);
        lv_obj_set_pos(lv_obj_get_parent(ui.st_val[i]), 0, 20);
    }

    /* Workbench readout (two lines) in the gap above the pager, not part of
     * the design: line 1 = last NRC, bus counters and LOOP time; line 2 = RPM next to the raw oil-pressure
     * candidate bytes. Filled in cluster_ui_refresh(). */
    ui.did_dbg = mk_label(pg, "", F12, C_FAINT);
    lv_obj_set_pos(ui.did_dbg, 10, 388);
    lv_obj_set_style_text_line_space(ui.did_dbg, 4, 0);

    mk_pager(pg, 1);
}

/* ---------------------------------------------------------------- SNIFF page */
/* Workbench page: lists the bus bytes that changed most recently, so operating
 * one control identifies the frame that carries it. Entering the page starts
 * listening and clears the history; leaving it stops. */
static void build_sniff(void)
{
    lv_obj_t *pg = ui.page[PAGE_SNIFF];

    mk_eyebrow(pg, "SNIFF");

    ui.sn_stat = mk_label(pg, "LISTENING...", F12, C_MUTED);
    lv_obj_set_pos(ui.sn_stat, 10, 40);

    /* STATE: low-cardinality bytes (selector / on-off flags), with value sets. */
    lv_obj_t *h1 = mk_label(pg, "STATE  ID.B N  VALUES", F12, C_FAINT);
    lv_obj_set_pos(h1, 10, 60);
    ui.sn_rows = mk_label(pg, "", F14, C_TEXT2);
    lv_obj_set_pos(ui.sn_rows, 10, 80);
    lv_obj_set_style_text_line_space(ui.sn_rows, 6, 0);

    /* ANALOG: widest-span bytes (temperatures, pressures), with min>max=now. */
    lv_obj_t *h2 = mk_label(pg, "ANALOG ID.B  MIN>MAX =NOW", F12, C_FAINT);
    lv_obj_set_pos(h2, 10, 214);
    ui.sn_ana = mk_label(pg, "", F14, C_TEXT2);
    lv_obj_set_pos(ui.sn_ana, 10, 234);
    lv_obj_set_style_text_line_space(ui.sn_ana, 6, 0);

    lv_obj_t *hint = mk_label(pg,
        "Selector: STATE row whose values are\n"
        "the detents. Temps/pressures: ANALOG,\n"
        "found on a warm-up / throttle blip.\n"
        "P/N only. Re-enter to clear. OBD paused.", F12, C_MUTED);
    lv_obj_set_pos(hint, 10, 380);
    lv_obj_set_style_text_line_space(hint, 4, 0);

    mk_pager(pg, PAGE_SNIFF);
}

/* --------------------------------------------------------------- alert strip */
/* Strip tags, left to right. 1..5 are measured values (TAG_M in refresh). */
static const char *const TAG_NAME[8] = { "MIL","CLT","OIL","ATF","EGT","BAT","DTC","CAN" };
static const char *const TAG_UNIT[8] = { "", DEG "C", DEG "C", DEG "C", DEG "C", "V", "", "" };
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

    for (int i = 0; i < 8; i++) {
        ui.tag[i] = mk_label(ui.strip, TAG_NAME[i], F12, C_FAINT);
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

    for (int i = 0; i < PAGE_COUNT; i++) {
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
    build_diag();
    build_sniff();
    build_strip(scr);

    /* Full-screen alert border: a transparent-centre ring on top of every page,
     * so a critical value or reverse gear is unmissable regardless of the page
     * shown. Hidden until refresh() decides a colour. Non-interactive: input is
     * the physical KEY button, so the overlay never needs to catch touches. */
    ui.border = lv_obj_create(scr);
    lv_obj_set_pos(ui.border, 0, 0);
    lv_obj_set_size(ui.border, 320, 480);
    lv_obj_set_style_bg_opa(ui.border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(ui.border, 0, 0);
    lv_obj_set_style_pad_all(ui.border, 0, 0);
    lv_obj_set_style_border_width(ui.border, 5, 0);
    lv_obj_set_style_border_color(ui.border, C_ORANGE, 0);
    lv_obj_set_style_border_opa(ui.border, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui.border, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.border, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.border, LV_OBJ_FLAG_HIDDEN);

    cluster_ui_set_page(0);
}

void cluster_ui_set_page(uint8_t p)
{
    if (p >= PAGE_COUNT) p = 0;
    s_page = p;
    /* Sniffing costs the OBD poller its bus access, so it is tied to the page
     * being visible: you cannot leave it running by accident. */
    can_sniff_set_active(p == PAGE_SNIFF);
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == p) lv_obj_clear_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* SNIFF pauses OBD polling, so it must not be reachable on the move (one
 * accidental tap while driving froze every OBD value). The selector broadcast
 * keeps arriving while sniffing, OBD replies do not, so the selector is the
 * gate: P or N only. Without a fresh selector (bench, another car) SNIFF is
 * allowed unless a fresh speed says the vehicle is moving. */
static bool sniff_allowed(void)
{
    uint32_t now  = HAL_GetTick();
    bool     live = g_obd.can_ok;
    if (live && obd_sel_fresh(&g_obd, now))
        return g_obd.sel_range == 1 || g_obd.sel_range == 3;     /* P / N */
    float sp = g_obd.speed;
    bool moving = live && !isnan(sp) && obd_is_fresh(&g_obd, M_SPEED, now) && sp > 3.0f;
    return !moving;
}

void cluster_ui_next_page(void)
{
    uint8_t next = (uint8_t)((s_page + 1u) % PAGE_COUNT);
    if (next == PAGE_SNIFF && !sniff_allowed()) next = 0;        /* skip to DRIVE */
    cluster_ui_set_page(next);
}

uint8_t cluster_ui_get_page(void)
{
    return s_page;
}

/* ------------------------------------------- change-only widget updates
 * In LVGL 9.3 lv_label_set_text() and every lv_obj_set_style_*() invalidate the
 * object even when the value is the same (lv_label.c, lv_obj_style.c), and
 * cluster_ui_refresh() runs at 25 Hz. Each invalidated area costs an SPI
 * transfer, so writing unchanged values repainted the visible page over and
 * over. These wrappers compare first. lv_bar_set_value() and the hidden flag
 * already skip no-op changes inside LVGL. A colour getter may return the
 * inherited or theme value when no local style is set yet; if that already
 * equals the wanted colour, the widget already looks right. */
static void ui_text(lv_obj_t *o, const char *t)
{
    const char *cur = lv_label_get_text(o);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(o, t);
}

static void ui_text_color(lv_obj_t *o, lv_color_t c)
{
    if (lv_color_eq(lv_obj_get_style_text_color(o, LV_PART_MAIN), c)) return;
    lv_obj_set_style_text_color(o, c, 0);
}

static void ui_bg_color(lv_obj_t *o, lv_color_t c, lv_style_selector_t part)
{
    if (lv_color_eq(lv_obj_get_style_bg_color(o, part), c)) return;
    lv_obj_set_style_bg_color(o, c, part);
}

static void ui_border_color(lv_obj_t *o, lv_color_t c)
{
    if (lv_color_eq(lv_obj_get_style_border_color(o, LV_PART_MAIN), c)) return;
    lv_obj_set_style_border_color(o, c, 0);
}

/* HAL tick of the refresh in progress, for the freshness checks. */
static uint32_t s_now;

/* Worth showing: link alive, a value ever decoded, and recent enough
 * (metric_stale_ms in cluster_config.h). Otherwise the UI shows "--". */
static bool shown(const obd_data_t *d, metric_key_t k, bool live)
{
    return live && has_value(mval(d, k)) && obd_is_fresh(d, k, s_now);
}

/* set one big value + optional bar + optional dot from a metric */
static void set_metric(lv_obj_t *val, lv_obj_t *bar, lv_obj_t *dot,
                       metric_key_t k, const obd_data_t *d, bool live)
{
    float v  = mval(d, k);
    bool  ok = shown(d, k, live);
    if (!ok) {
        ui_text(val, "--");
        ui_text_color(val, C_MUTED);
        if (dot) ui_bg_color(dot, C_FAINT, 0);
        if (bar) {
            lv_bar_set_value(bar, 0, LV_ANIM_OFF);
            ui_bg_color(bar, C_FAINT, LV_PART_INDICATOR);
        }
        return;
    }
    char b[16];
    fmt(b, sizeof b, v, metrics[k].decimals);
    ui_text(val, b);

    metric_state_t s = metric_state(k, v);
    lv_color_t c = state_color(s);
    /* Cold temperature (engine warming up): light blue instead of green. */
    if (s == ST_OK && is_temp(k) && v < 50.0f) c = C_COLD;
    /* Vivid state colouring, matching the design reference: the value takes
     * its state colour (OK=green, INFO=blue, WARN=amber, CRIT=red). The
     * saturated colour is what carries contrast on this weak panel. */
    ui_text_color(val, c);
    if (dot) ui_bg_color(dot, c, 0);
    if (bar) {
        lv_bar_set_value(bar, pct_of(k, v), LV_ANIM_OFF);
        ui_bg_color(bar, c, LV_PART_INDICATOR);
    }
}

void cluster_ui_refresh(void)
{
    /* snapshot the volatile shared struct */
    obd_data_t d;
    d.speed = g_obd.speed; d.rpm = g_obd.rpm; d.cool = g_obd.cool;
    d.oil = g_obd.oil; d.iat = g_obd.iat; d.load = g_obd.load;
    d.boost = g_obd.boost; d.rail = g_obd.rail; d.egt = g_obd.egt;
    d.battery = g_obd.battery; d.atf = g_obd.atf;
    d.gear = g_obd.gear; d.sel_range = g_obd.sel_range;
    d.mil = g_obd.mil; d.dtc_count = g_obd.dtc_count; d.can_ok = g_obd.can_ok;
    d.mil_valid = g_obd.mil_valid;
    d.gear_raw = g_obd.gear_raw; d.oil_press = g_obd.oil_press;
    d.last_nrc_sid = g_obd.last_nrc_sid; d.last_nrc = g_obd.last_nrc;
    d.oilp_1ba_raw = g_obd.oilp_1ba_raw; d.oilp_0c9_raw = g_obd.oilp_0c9_raw;
    for (int k = 0; k < M_COUNT; k++) d.upd_ms[k] = g_obd.upd_ms[k];
    d.sel_upd_ms = g_obd.sel_upd_ms;
    bool live = d.can_ok;
    s_now = HAL_GetTick();
    /* Leave SNIFF by itself once the selector goes into R/D (see sniff_allowed). */
    if (s_page == PAGE_SNIFF && !sniff_allowed()) cluster_ui_set_page(0);
    /* The selector broadcast has its own freshness; a stale one is "--",
     * and so is the gear number in D. */
    bool sel_ok = live && obd_sel_fresh(&d, s_now);

    char b[16];

    /* ----- DRIVE ----- */
    /* GEAR: real selector range from the 0x1F5 broadcast (byte 3), found with
     * the SNIFF page -- 1 P / 2 R / 3 N / 4 D (docs/sniff-selector.md). The
     * design shows the drive gear in nominal green; P/R/N stay white. The old
     * 0x199A DID was the engaged gear RATIO and could not express P/R/N. */
    switch (sel_ok ? d.sel_range : -1) {
        case 1:  lv_snprintf(b, sizeof b, "P"); break;
        case 2:  lv_snprintf(b, sizeof b, "R"); break;
        case 3:  lv_snprintf(b, sizeof b, "N"); break;
        case 4:  /* D + engaged gear (0x199A) as the design's white letter +
                  * green number; bare "D" until the gear number arrives */
                 if (d.gear >= 1 && obd_is_fresh(&d, M_GEAR, s_now))
                     lv_snprintf(b, sizeof b, "D#37d67a %d#", (int)d.gear);
                 else             lv_snprintf(b, sizeof b, "D");
                 break;
        default: lv_snprintf(b, sizeof b, "--"); break;
    }
    ui_text(ui.gear_val, b);
    /* Reverse: the whole "R" glows bright orange (a reversing cue). Other ranges
     * keep the white base; the D case still recolors its gear number green. */
    ui_text_color(ui.gear_val, (sel_ok && d.sel_range == 2) ? C_ORANGE : C_TEXT2);

    if (shown(&d, M_SPEED, live)) { fmt(b, sizeof b, d.speed, 0); ui_text(ui.speed_val, b); }
    else        ui_text(ui.speed_val, "--");
    if (shown(&d, M_RPM, live)) { fmt(b, sizeof b, d.rpm, 0); ui_text(ui.rpm_val, b); }
    else        ui_text(ui.rpm_val, "--");
    {   /* RPM stays white, but warns/reds near the redline */
        metric_state_t rs = shown(&d, M_RPM, live) ? metric_state(M_RPM, d.rpm) : ST_OK;
        ui_text_color(ui.rpm_val, (rs >= ST_WARN) ? state_color(rs) : C_TEXT2);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.dm_val[i], ui.dm_bar[i], ui.dm_dot[i], DRIVE_M[i], &d, live);
    set_metric(ui.boost_val, NULL, NULL, M_BOOST, &d, live);
    /* MIL and CAN state now live only in the rail (see alert strip below). */

    /* ----- DIAG -----
     * MIL and the DTC count both come from PID 0x01. Until it has answered,
     * mil=false / dtc_count=0 are just initial values, so show "--" / NO DATA
     * rather than a reassuring OFF / no-codes. The count is the emission-related
     * DTC count only (the mode-03 list is not decoded), hence the wording. */
    bool mil_ok = live && d.mil_valid;
    if (!mil_ok) {
        ui_text(ui.mil_text, "--");
        ui_text_color(ui.mil_text, C_MUTED);
    } else {
        ui_text(ui.mil_text, d.mil ? "ON" : "OFF");
        ui_text_color(ui.mil_text, d.mil ? C_CRIT : C_TEXT2);
    }
    if (!mil_ok) {
        ui_text(ui.dtc_msg, "NO DATA");
        ui_text_color(ui.dtc_msg, C_MUTED);
        ui_text(ui.dtc_ic, "-");
        ui_text_color(ui.dtc_ic, C_MUTED);
        ui_border_color(ui.dtc_ring, C_FAINT);
    } else if (d.dtc_count > 0) {
        char mb[40];
        lv_snprintf(mb, sizeof mb, "%u EMISSION CODE%s",
                    (unsigned)d.dtc_count, d.dtc_count == 1 ? "" : "S");
        ui_text(ui.dtc_msg, mb);
        ui_text_color(ui.dtc_msg, C_WARN);
        ui_text(ui.dtc_ic, "!");
        ui_text_color(ui.dtc_ic, C_WARN);
        ui_border_color(ui.dtc_ring, C_WARN);
    } else {
        ui_text(ui.dtc_msg, "NO EMISSION CODES");
        ui_text_color(ui.dtc_msg, C_OK);
        ui_text(ui.dtc_ic, LV_SYMBOL_OK);
        ui_text_color(ui.dtc_ic, C_OK);
        ui_border_color(ui.dtc_ring, C_OK);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.st_val[i], NULL, NULL, STAT_M[i], &d, live);

    {   /* Workbench readout. Line 1: last negative response, RX FIFO overflow
         *         events, frames dropped as malformed, TX failures, and LOOP =
         *         the longest superloop period over the last second in ms (how
         *         long the display path stalls OBD) — whether the normal
         *         DRIVE/DIAG traffic overruns the 16-deep FIFO during such a
         *         stall is an open question worth a photo.
         * Line 2: live RPM next to the two raw oil-pressure candidate bytes, so
         *         ONE photo during a throttle blip shows which byte tracks
         *         engine speed (docs/oil-pressure-test.md). Both are UNVERIFIED:
         *         shown raw only, never decoded into the OIL P tile or an alarm. */
        char db[96];
        char nrc[16];
        if (d.last_nrc_sid)
            lv_snprintf(nrc, sizeof nrc, "%02X/%02X",
                        (unsigned)d.last_nrc_sid, (unsigned)d.last_nrc);
        else
            lv_snprintf(nrc, sizeof nrc, "--");
        obd_health_t h;
        obd_can_health(&h);
        int o = lv_snprintf(db, sizeof db, "NRC %s LOST %u BAD %u TXF %u LOOP %u\n",
                            nrc, (unsigned)h.rx_lost, (unsigned)h.rx_bad,
                            (unsigned)h.tx_fail, (unsigned)app_loop_max_ms());

        int rpm = has_value(d.rpm) ? (int)d.rpm : 0;
        o += lv_snprintf(db + o, sizeof db - o, "RPM %d  1BA.3=%02X  0C9.2=%02X",
                         rpm, (unsigned)d.oilp_1ba_raw, (unsigned)d.oilp_0c9_raw);
        /* Aborted display SPI transfers (main / status screen): only shown
         * when something went wrong, so the line stays short normally. */
        uint16_t se = lv_port_disp_spi_errors(), se4 = st7735_status_spi_errors();
        if ((se || se4) && o < (int)sizeof db)
            lv_snprintf(db + o, sizeof db - o, " SPI %u/%u", (unsigned)se, (unsigned)se4);
        ui_text(ui.did_dbg, db);
    }

    /* ----- SNIFF -----
     * Only while visible: the candidate/mover views scan the whole table and
     * the page is hidden the rest of the time. */
    if (s_page == PAGE_SNIFF) {
        /* Lead with the frame rate and bus health: a frozen row table means
         * nothing if you cannot see whether frames are still arriving. fps 0 =
         * bus asleep (procedural, not a bug); BOFF/EP/LOST = a real fault. */
        obd_health_t h;
        obd_can_health(&h);
        /* FILTER ERR: the wide acceptance filter was refused, so nothing is
         * being sniffed (and OBD polling was left running). */
        bool ferr = can_sniff_filter_error();
        char sb[64];
        /* POLL: the FDCAN RX interrupt could not be enabled, frames are
         * drained from the loop (the old, overflow-prone behaviour). */
        lv_snprintf(sb, sizeof sb, "%s%s%u fps  IDS %u  %s L%u",
                    ferr ? "FILTER ERR " : "", h.rx_irq ? "" : "POLL ",
                    (unsigned)can_sniff_fps(), (unsigned)can_sniff_id_count(),
                    h.bus_off ? "BUSOFF" : (h.err_passive ? "ERRPASS" : "ok"),
                    (unsigned)h.rx_lost);
        ui_text(ui.sn_stat, sb);
        ui_text_color(ui.sn_stat, (h.bus_off || ferr) ? C_CRIT
                                  : (can_sniff_fps() ? C_OK : C_WARN));

        /* STATE: low-cardinality bytes with their distinct value set. The
         * selector is the row whose values are the detent codes, listed in
         * first-seen (== swept) order; counters are excluded by saturation. */
        sniff_cand_t cs[6];
        uint8_t  nc = can_sniff_candidates(cs, 6);
        char rows[6 * 40 + 2];
        int  off = 0;
        for (uint8_t i = 0; i < nc; i++) {
            off += lv_snprintf(rows + off, sizeof rows - off,
                               "%03X.%u %u ", (unsigned)cs[i].id,
                               (unsigned)cs[i].byte_idx, (unsigned)cs[i].nvals);
            for (uint8_t k = 0; k < cs[i].nvals && off < (int)sizeof rows - 4; k++)
                off += lv_snprintf(rows + off, sizeof rows - off,
                                   "%02X", (unsigned)cs[i].vals[k]);
            off += lv_snprintf(rows + off, sizeof rows - off, "\n");
            if (off >= (int)sizeof rows - 1) break;
        }
        if (nc == 0) lv_snprintf(rows, sizeof rows, "no state bytes yet");
        ui_text(ui.sn_rows, rows);

        /* ANALOG: widest-span bytes. A temperature climbing on warm-up or a
         * pressure jumping on a throttle blip shows a large min>max here while
         * being hidden from STATE (too many distinct values). */
        sniff_mover_t mv[5];
        uint8_t  nm = can_sniff_movers(mv, 5);
        char arows[5 * 32 + 2];
        int  aoff = 0;
        for (uint8_t i = 0; i < nm; i++) {
            aoff += lv_snprintf(arows + aoff, sizeof arows - aoff,
                                "%03X.%u  %02X>%02X =%02X\n",
                                (unsigned)mv[i].id, (unsigned)mv[i].byte_idx,
                                (unsigned)mv[i].vmin, (unsigned)mv[i].vmax,
                                (unsigned)mv[i].cur);
            if (aoff >= (int)sizeof arows - 1) break;
        }
        if (nm == 0) lv_snprintf(arows, sizeof arows, "no analog bytes yet");
        ui_text(ui.sn_ana, arows);
    }

    /* ----- alert strip -----
     * A tag only judges a value we actually have and that is recent:
     * metric_state() on NaN would fall through every comparison and report a
     * confident ST_OK, and a PID that reads 0 because it is unsupported must
     * not raise CHECK either. A tag with nothing to judge is drawn neutral,
     * not as a green "healthy". */
    static const metric_key_t TAG_M[8] = { 0, M_COOL, M_OIL, M_ATF, M_EGT, M_BATTERY, 0, 0 };
    metric_state_t ts[8];
    bool known[8];
    for (int i = 1; i <= 5; i++) {
        known[i] = shown(&d, TAG_M[i], live);
        ts[i]    = known[i] ? metric_state(TAG_M[i], mval(&d, TAG_M[i])) : ST_OK;
    }
    known[0] = mil_ok; ts[0] = (mil_ok && d.mil) ? ST_CRIT : ST_OK;            /* MIL */
    known[6] = mil_ok; ts[6] = (mil_ok && d.dtc_count > 0) ? ST_WARN : ST_OK;  /* DTC */
    known[7] = true;   ts[7] = live ? ST_OK : ST_CRIT;                         /* CAN */

    int worst = ST_OK, nalarm = 0;
    for (int i = 0; i < 8; i++) {
        /* MIL (0) and DTC (6) are indicators: grey/dim when nominal, not green.
         * The monitored systems glow green when healthy (design signature). */
        bool indicator = (i == 0 || i == 6);
        lv_color_t tc, ulc;
        if      (ts[i] == ST_CRIT) { tc = C_CRIT; ulc = C_CRIT; }
        else if (ts[i] == ST_WARN) { tc = C_WARN; ulc = C_WARN; }
        else if (!known[i])        { tc = C_FAINT; ulc = C_LINE; }
        else                       { tc  = indicator ? C_FAINT : C_MUTED;
                                     ulc = indicator ? C_LINE  : C_OK; }
        ui_text_color(ui.tag[i], tc);
        ui_bg_color(ui.tag_ul[i], ulc, 0);
        if (ts[i] > worst) worst = ts[i];
        if (ts[i] >= ST_WARN) nalarm++;
    }

    /* Name the alarm instead of a bare CHECK/WARN: the most severe tag (first
     * in strip order on a tie), with its reading where it has one. */
    int cause = -1;
    for (int i = 0; i < 8; i++)
        if (ts[i] >= ST_WARN && (cause < 0 || ts[i] > ts[cause])) cause = i;
    /* The link can be "live" on broadcasts alone while the ECM answers
     * nothing: then there is nothing to call NOMINAL. RPM (0 with the engine
     * off) or coolant fresh means the ECM is answering. */
    bool ecm_fresh = shown(&d, M_RPM, live) || shown(&d, M_COOL, live);

    char sum[28];
    const char *txt = sum; lv_color_t col;
    if (!live) {
        txt = LV_SYMBOL_WARNING " CAN LOST"; col = C_CRIT;
    } else if (cause >= 0) {
        col = (ts[cause] == ST_CRIT) ? C_CRIT : C_WARN;
        if (cause >= 1 && cause <= 5) {
            char v[12];
            fmt(v, sizeof v, mval(&d, TAG_M[cause]), metrics[TAG_M[cause]].decimals);
            lv_snprintf(sum, sizeof sum, LV_SYMBOL_WARNING " %s %s%s",
                        TAG_NAME[cause], v, TAG_UNIT[cause]);
        } else if (cause == 0) {
            lv_snprintf(sum, sizeof sum, LV_SYMBOL_WARNING " MIL ON");
        } else {
            lv_snprintf(sum, sizeof sum, LV_SYMBOL_WARNING " %u DTC", (unsigned)d.dtc_count);
        }
    } else if (!ecm_fresh) {
        txt = "NO ECM DATA"; col = C_WARN;
    } else {
        txt = "NOMINAL"; col = C_OK;
    }
    ui_text(ui.summary, txt);
    ui_text_color(ui.summary, col);

    if (nalarm > 0) { lv_snprintf(b, sizeof b, "%d", nalarm); ui_text(ui.count, b); }
    else            ui_text(ui.count, "");

    /* strip background tint by worst state */
    lv_color_t sbg = C_ALERTBG, sbd = C_LINE;
    if (worst == ST_CRIT || !live) { sbg = lv_color_hex(0x1a0a0d); sbd = C_CRIT; }
    else if (worst == ST_WARN || !ecm_fresh) { sbg = lv_color_hex(0x1a1408); sbd = C_WARN; }
    ui_bg_color(ui.strip, sbg, 0);
    ui_border_color(ui.strip, sbd);

    /* full-screen alert border: red on any critical parameter (high coolant,
     * MIL, CAN lost, ...), orange when reverse is engaged, off otherwise. Red
     * outranks reverse. */
    if (worst == ST_CRIT || !live) {
        ui_border_color(ui.border, C_CRIT);
        lv_obj_clear_flag(ui.border, LV_OBJ_FLAG_HIDDEN);
    } else if (sel_ok && d.sel_range == 2) {        /* R */
        ui_border_color(ui.border, C_ORANGE);
        lv_obj_clear_flag(ui.border, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.border, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Called by fdcan_obd.c whenever a decoded value changes. Nothing to do: the
 * refresh reads g_obd at ~25 Hz regardless (freshness can expire without any
 * new frame), and the change-only ui_* helpers make an unchanged refresh
 * cheap. Kept as the hook so a future event-driven UI has one place to start. */
void obd_on_update(void)
{
}
