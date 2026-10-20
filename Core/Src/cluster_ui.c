/* ============================================================================
 *  cluster_ui.c — LVGL 9 UI for the auxiliary display (LC100, Colorado 2.8 E98 swap).
 *
 *  Port of the visual reference
 *      design/Diesel_Cluster_v2_3.html
 *  to LVGL primitives (containers / labels / bars / arc). Palette, geometry
 *  and typography follow the HTML; the SVG line-art icons are represented by
 *  compact text tags (LVGL has no vector-icon set). Where the firmware
 *  departs from the mockup on purpose (DRIVE has no eyebrow line, the fourth
 *  temperature tile is OIL P ? instead of EGT, the PRNDL letter comes from the
 *  0x1F5 broadcast) is listed in docs/REVIEW.md, review C7.
 *
 *  Layout is absolute-positioned on a 320x480 panel:
 *      y 0..31    alert strip (always visible)
 *      y 32..479  the active page (DRIVE / DIAG / SNIFF), 320 x 448
 *
 *  Values come from g_obd (fdcan_obd.c). "no data" (bus quiet, or an enhanced
 *  DID not yet mapped) renders as "--" in grey — never a fake green reading.
 * ========================================================================== */

#include "cluster_ui.h"
#include "cluster_config.h"
#include "fdcan_obd.h"
#include "gps.h"
#include "cluster_policy.h"   /* shown / SNIFF gate / summary decisions */
#include "can_sniff.h"
#include "fault.h"
#include "app_main.h"         /* app_loop_max_ms() for the DIAG readout */
#include "lv_port_disp.h"     /* SPI error counters for the DIAG readout */
#include "st7735_status.h"
#include "fw_info.h"          /* image id + variant at the foot of DIAG */
#include "alert.h"            /* filtered alarm states                  */
#include "lvgl.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

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

/* "Do we actually have this number?" is policy_fresh() (cluster_policy.h):
 * g_obd initialises every float to NaN and only a decoded frame overwrites it,
 * so NaN covers both unmapped DIDs and PIDs this calibration does not support.
 * Blanket-blacklisting metrics (the old is_enhanced()) hid real data the moment
 * a DID started working. */

/* the °C gauge temperatures — get a "cold" blue tint below 50°C */
static bool is_temp(metric_key_t k)
{
    return k == M_COOL || k == M_OIL || k == M_ATF || k == M_EGT;
}

static float mval(const obd_data_t *d, metric_key_t k) { return policy_value(d, k); }

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

    /* "OIL P ?": the source (PID 0xA22C) is not yet validated on this engine
     * (OILP_VALIDATED 0) — the number is information, not a checked reading. */
    static const char *NAMES[4] = { "COOLANT", "OIL", "ATF",
                                    OILP_VALIDATED ? "OIL P" : "OIL P ?" };
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
    char rb[40];
    /* xN: N abnormal resets in a row — a reset storm, not a one-off. */
    if (fault_reset_streak() > 1u)
        lv_snprintf(rb, sizeof rb, "LAST RESET: %s x%u", fault_reset_text(),
                     (unsigned)fault_reset_streak());
    else
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

    /* Workbench readout (three lines) in the gap between the cards (end at
     * 380) and the pager (432), not part of the design: 3 x 15 px + 2 x 1 px
     * from 382 ends at 429. Filled in cluster_ui_refresh(). */
    ui.did_dbg = mk_label(pg, "", F12, C_FAINT);
    lv_obj_set_pos(ui.did_dbg, 10, 382);
    lv_obj_set_style_text_line_space(ui.did_dbg, 1, 0);

    mk_pager(pg, 1);

    /* Which image this is (fw_info.h), either side of the page dots: commit
     * (+ = sources changed since) on the left, variant on the right, amber
     * for an image that is not for driving. */
    lv_obj_t *fid = mk_label(pg, FW_ID, F12, C_FAINT);
    lv_obj_set_pos(fid, 10, 430);
    lv_obj_t *fvar = mk_label(pg, FW_VARIANT, F12, FW_BENCH ? C_WARN : C_FAINT);
    lv_obj_align(fvar, LV_ALIGN_TOP_RIGHT, -8, 430);
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

static bool sniff_allowed(void);

void cluster_ui_set_page(uint8_t p)
{
    if (p >= PAGE_COUNT) p = 0;
    if (p == PAGE_SNIFF && !sniff_allowed()) p = 0;   /* any caller, not only next_page */
    s_page = p;
    /* Sniffing costs the OBD poller its bus access, so it is tied to the page
     * being visible: you cannot leave it running by accident. */
    /* If the wide filter is refused the page still opens, but says so
     * (NOT LISTENING) and refresh() retries; see can_sniff_set_active(). */
    (void)can_sniff_set_active(p == PAGE_SNIFF);
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == p) lv_obj_clear_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(ui.page[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* SNIFF pauses OBD polling, so it must not be reachable on the move (one
 * accidental tap while driving froze every OBD value): fresh P only,
 * fail closed — see policy_sniff_allowed(). SNIFF_BENCH_OVERRIDE opens it on
 * the bench without a selector. */
static bool sniff_allowed(void)
{
    return policy_sniff_allowed(&g_obd, HAL_GetTick());
}

void cluster_ui_next_page(void)
{
    uint8_t next = (uint8_t)((s_page + 1u) % PAGE_COUNT);
    if (next == PAGE_SNIFF && !sniff_allowed()) next = 0;        /* skip to DRIVE */
    cluster_ui_set_page(next);
}

void cluster_ui_prev_page(void)
{
    uint8_t prev = (uint8_t)((s_page + PAGE_COUNT - 1u) % PAGE_COUNT);
    if (prev == PAGE_SNIFF && !sniff_allowed())                  /* skip to DIAG */
        prev = (uint8_t)((PAGE_SNIFF + PAGE_COUNT - 1u) % PAGE_COUNT);
    cluster_ui_set_page(prev);
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

/* Append a formatted piece to a fixed buffer, never past its end. Adding
 * lv_snprintf()'s return value (the length it WANTED) to the offset used to
 * push the offset beyond the buffer once one field was cut, which then
 * silently dropped every later field — the error counters first. */
static void cat(char *b, size_t n, size_t *o, const char *fmt, ...)
{
    if (*o + 1u >= n) return;
    va_list ap;
    va_start(ap, fmt);
    int w = lv_vsnprintf(b + *o, n - *o, fmt, ap);
    va_end(ap);
    if (w < 0) return;
    *o += (size_t)w;
    if (*o >= n) *o = n - 1u;
}

/* 8 hex digits of a PID support map, or "--------" while it is not read. */
static void hex8(char out[9], uint32_t v, bool have)
{
    static const char H[] = "0123456789ABCDEF";
    if (!have) { memcpy(out, "--------", 9); return; }
    for (int i = 7; i >= 0; i--) { out[i] = H[v & 15u]; v >>= 4; }
    out[8] = '\0';
}

/* HAL tick of the refresh in progress, for the freshness checks. */
static uint32_t s_now;

/* Worth showing: link alive, a value ever decoded, and recent enough
 * (metric_stale_ms in cluster_config.h). Otherwise the UI shows "--". */
static bool shown(const obd_data_t *d, metric_key_t k, bool live)
{
    return live && policy_fresh(d, k, s_now);
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

    /* Judged metrics take their filtered state (alert.c): dwell, hysteresis,
     * and ST_INFO (neutral) while not judged — battery and oil pressure with
     * the engine not running, oil pressure until OILP_VALIDATED. It is not
     * on the alert strip either way. The rest keep their plain state. */
    metric_state_t s = alert_cfg[k].used ? alerts_state(k) : metric_state(k, v);
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
    d.mil_valid = g_obd.mil_valid; d.mil_upd_ms = g_obd.mil_upd_ms;
    d.gear_raw = g_obd.gear_raw; d.oil_press = g_obd.oil_press;
    d.last_nrc_sid = g_obd.last_nrc_sid; d.last_nrc = g_obd.last_nrc;
    for (int k = 0; k < M_COUNT; k++) d.upd_ms[k] = g_obd.upd_ms[k];
    d.sel_upd_ms = g_obd.sel_upd_ms;
    bool live = d.can_ok;
    s_now = HAL_GetTick();
    /* Filtered alarm states (dwell, hysteresis, engine-running gate) for
     * every judged metric, used below for value colours and the strip. */
    alerts_update(&d, s_now);
    /* Leave SNIFF by itself once the selector goes into R/D (see sniff_allowed). */
    if (s_page == PAGE_SNIFF && !sniff_allowed()) cluster_ui_set_page(0);
    /* On SNIFF but not listening (wide filter refused): try again once a
     * second, so the page converges on the state it shows. */
    if (s_page == PAGE_SNIFF && !can_sniff_is_active()) {
        static uint32_t t_retry;
        if (s_now - t_retry >= 1000u) { t_retry = s_now; (void)can_sniff_set_active(true); }
    }
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
                 if (d.gear >= 1 && policy_fresh(&d, M_GEAR, s_now))
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
        metric_state_t rs = alerts_state(M_RPM);       /* ST_INFO when not shown */
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
     * DTC count only (the mode-03 list is not decoded), and from the ECM only
     * (mode 01 is accepted from 0x7E8 alone): the TCM, ABS and SRS keep their
     * own codes, so the wording names the source (review C7 CAN-05). */
    bool mil_ok = live && obd_mil_fresh(&d, s_now);
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
        lv_snprintf(mb, sizeof mb, "ECM: %u EMISSION CODE%s",
                    (unsigned)d.dtc_count, d.dtc_count == 1 ? "" : "S");
        ui_text(ui.dtc_msg, mb);
        ui_text_color(ui.dtc_msg, C_WARN);
        ui_text(ui.dtc_ic, "!");
        ui_text_color(ui.dtc_ic, C_WARN);
        ui_border_color(ui.dtc_ring, C_WARN);
    } else {
        ui_text(ui.dtc_msg, "ECM: NO EMISSION CODES");
        ui_text_color(ui.dtc_msg, C_OK);
        ui_text(ui.dtc_ic, LV_SYMBOL_OK);
        ui_text_color(ui.dtc_ic, C_OK);
        ui_border_color(ui.dtc_ring, C_OK);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.st_val[i], NULL, NULL, STAT_M[i], &d, live);

    {   /* Workbench readout, three lines, each at most ~40 characters in
         * the worst case so it fits the 300 px label (montserrat 12 ~7 px a
         * glyph). Composed with cat(), which never steps past the buffer.
         * Line 1: last negative response; LOOP = the longest superloop period
         *         over the last second in ms (how long the display path
         *         stalls OBD, shown up to 9999); Q = the most frames ever
         *         waiting in the 256-deep RX ring; then, only when non-zero,
         *         CAP (transactions cut by the 10 s total limit), TXL (requests
         *         sent once and never on the bus: lost arbitration or a bus
         *         error, no automatic retransmission) and WIDE (SNIFF is off
         *         but the narrow filter is not restored yet).
         * Line 2: RX FIFO/ring loss, frames dropped as malformed, TX refusals,
         *         then GPS: - none found, ? searching, V<sats> running without
         *         a fix, <sats>/<km/h> with a fresh fix; O<n> when GPS bytes
         *         were lost (UART overrun or a full ring).
         * Line 3: the oil-pressure probe (PID 0xA22C) — path 22 / 2C / AA /
         *         NONE, the last NRC of each service, the last raw byte
         *         (A x 4 kPa); docs/oil-pressure-test.md. If a display SPI
         *         error was ever seen, it leads this line instead (main/status
         *         errors, panel re-inits, DMA fallbacks; each up to 9999) and
         *         the probe shortens to path + raw.
         *         Line 3 rotates every 4 s with two more views: the PID support
         *         maps (mode 01 PIDs 00/20/40, then 60/80/A0; 8 hex digits each,
         *         "--------" = not read) and whether engine fuel rate 0x5E is
         *         supported (+ yes, - no, ? unknown). One photo of each settles
         *         what this ECM offers for fuel consumption. */
        char db[160];
        size_t o = 0;
        char nrc[8] = "--";
        if (d.last_nrc_sid)
            lv_snprintf(nrc, sizeof nrc, "%02X/%02X",
                        (unsigned)d.last_nrc_sid, (unsigned)d.last_nrc);
        obd_health_t h;
        obd_can_health(&h);
        uint32_t loop = app_loop_max_ms();
        cat(db, sizeof db, &o, "NRC %s LOOP %u Q%u", nrc,
            (unsigned)(loop > 9999u ? 9999u : loop), (unsigned)h.rx_hwm);
        if (h.txn_capped)             cat(db, sizeof db, &o, " CAP%u", (unsigned)h.txn_capped);
        if (h.tx_lost)                cat(db, sizeof db, &o, " TXL%u",
                                          (unsigned)(h.tx_lost > 999u ? 999u : h.tx_lost));
        if (can_sniff_restore_pending()) cat(db, sizeof db, &o, " WIDE");
        char gt[16];
        const gps_state_t *gs = gps_state();
        if (gs->link == GPS_LINK_ABSENT)   lv_snprintf(gt, sizeof gt, "-");
        else if (gs->link != GPS_LINK_RUN) lv_snprintf(gt, sizeof gt, "?");
        else if (!gps_fix_fresh(s_now) || isnan(gs->fix.speed_kmh))
            lv_snprintf(gt, sizeof gt, "V%u", (unsigned)gs->fix.sats);
        else {
            unsigned v10 = (unsigned)(gs->fix.speed_kmh * 10.0f + 0.5f);
            lv_snprintf(gt, sizeof gt, "%u/%u.%u", (unsigned)gs->fix.sats, v10 / 10u, v10 % 10u);
        }
        if (gs->overruns) {           /* bytes lost: UART overrun or a full ring */
            size_t gl = strlen(gt);
            lv_snprintf(gt + gl, sizeof gt - gl, " O%u",
                        (unsigned)(gs->overruns > 999u ? 999u : gs->overruns));
        }
        cat(db, sizeof db, &o, "\nLOST %u BAD %u TXF %u GPS %s\n",
            (unsigned)h.rx_lost, (unsigned)h.rx_bad, (unsigned)h.tx_fail, gt);

        static const char *const OILP_PATH[] = { "22", "2C", "AA", "NONE" };
        obd_oilp_probe_t op;
        obd_oilp_probe(&op);
        const char *path = OILP_PATH[op.mode <= OILP_NONE ? op.mode : OILP_NONE];
        char n22[4] = "--", n2c[4] = "--", naa[4] = "--", raw[4] = "--";
        if (op.nrc22) lv_snprintf(n22, sizeof n22, "%02X", (unsigned)op.nrc22);
        if (op.nrc2c) lv_snprintf(n2c, sizeof n2c, "%02X", (unsigned)op.nrc2c);
        if (op.nrcaa) lv_snprintf(naa, sizeof naa, "%02X", (unsigned)op.nrcaa);
        if (op.have_raw) lv_snprintf(raw, sizeof raw, "%02X", (unsigned)op.raw);
        unsigned se = lv_port_disp_spi_errors(), se4 = st7735_status_spi_errors();
        unsigned fb = lv_port_disp_dma_fallbacks(), rc = lv_port_disp_recoveries();
        unsigned l3 = (unsigned)((s_now / 4000u) % 3u);
        if (l3 != 0u) {
            obd_pid_support_t ps;
            obd_pid_support(&ps);
            uint8_t r0 = (l3 == 1u) ? 0u : 3u;
            char m[3][9];
            for (uint8_t j = 0; j < 3u; j++)
                hex8(m[j], ps.mask[r0 + j], ((ps.have >> (r0 + j)) & 1u) != 0u);
            if (l3 == 1u) {
                cat(db, sizeof db, &o, "PID 00:%s 20:%s 40:%s", m[0], m[1], m[2]);
            } else {
                int8_t f = obd_pid_supported(0x5E);
                cat(db, sizeof db, &o, "60:%s 80:%s A0:%s 5E%c", m[0], m[1], m[2],
                    f > 0 ? '+' : (f == 0 ? '-' : '?'));
            }
        } else if (se || se4 || fb) {
            #define C4(v) ((v) > 9999u ? 9999u : (v))
            cat(db, sizeof db, &o, "SPI %u/%u R%u F%u OILP %s RAW %s",
                C4(se), C4(se4), C4(rc), C4(fb), path, raw);
            #undef C4
        } else {
            cat(db, sizeof db, &o, "OILP %s 22/%s 2C/%s AA/%s RAW %s",
                path, n22, n2c, naa, raw);
        }
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
        /* NOT LISTENING: the wide acceptance filter was refused, so nothing
         * is being sniffed and OBD polling keeps running (retried 1/s above).
         * The page must not look like a quiet bus. */
        bool listening = can_sniff_is_active();
        char sb[64];
        /* POLL: the FDCAN RX interrupt could not be enabled, frames are
         * drained from the loop (the old, overflow-prone behaviour). */
        if (!listening)
            lv_snprintf(sb, sizeof sb, "FILTER ERR: NOT LISTENING");
        else
            lv_snprintf(sb, sizeof sb, "%s%u fps  IDS %u  %s L%u",
                        h.rx_irq ? "" : "POLL ",
                        (unsigned)can_sniff_fps(), (unsigned)can_sniff_id_count(),
                        h.bus_off ? "BUSOFF" : (h.err_passive ? "ERRPASS" : "ok"),
                        (unsigned)h.rx_lost);
        ui_text(ui.sn_stat, sb);
        ui_text_color(ui.sn_stat, (h.bus_off || !listening) ? C_CRIT
                                  : (can_sniff_fps() ? C_OK : C_WARN));

        /* STATE: low-cardinality bytes with their distinct value set. The
         * selector is the row whose values are the detent codes, listed in
         * first-seen (== swept) order; counters are excluded by saturation. */
        sniff_cand_t cs[6];
        uint8_t  nc = can_sniff_candidates(cs, 6);
        char rows[6 * 40 + 2];
        size_t off = 0;               /* cat(): a cut row cannot push off past
                                       * the end (review C7 SW-03)            */
        for (uint8_t i = 0; i < nc; i++) {
            cat(rows, sizeof rows, &off, "%03X.%u %u ", (unsigned)cs[i].id,
                (unsigned)cs[i].byte_idx, (unsigned)cs[i].nvals);
            for (uint8_t k = 0; k < cs[i].nvals; k++)
                cat(rows, sizeof rows, &off, "%02X", (unsigned)cs[i].vals[k]);
            cat(rows, sizeof rows, &off, "\n");
        }
        if (nc == 0) lv_snprintf(rows, sizeof rows, "no state bytes yet");
        ui_text(ui.sn_rows, rows);

        /* ANALOG: widest-span bytes. A temperature climbing on warm-up or a
         * pressure jumping on a throttle blip shows a large min>max here while
         * being hidden from STATE (too many distinct values). */
        sniff_mover_t mv[5];
        uint8_t  nm = can_sniff_movers(mv, 5);
        char arows[5 * 32 + 2];
        size_t aoff = 0;
        for (uint8_t i = 0; i < nm; i++)
            cat(arows, sizeof arows, &aoff, "%03X.%u  %02X>%02X =%02X\n",
                (unsigned)mv[i].id, (unsigned)mv[i].byte_idx,
                (unsigned)mv[i].vmin, (unsigned)mv[i].vmax, (unsigned)mv[i].cur);
        if (nm == 0) lv_snprintf(arows, sizeof arows, "no analog bytes yet");
        ui_text(ui.sn_ana, arows);
    }

    /* ----- alert strip -----
     * A tag only judges a value we actually have and that is recent:
     * metric_state() on NaN would fall through every comparison and report a
     * confident ST_OK, and a PID that reads 0 because it is unsupported must
     * not raise CHECK either. A tag with nothing to judge is drawn neutral,
     * not as a green "healthy" — that includes the battery while the engine
     * is not running (alert.c judges it only then). States are the filtered
     * ones: a value at a threshold does not flicker the strip. */
    static const metric_key_t TAG_M[8] = { 0, M_COOL, M_OIL, M_ATF, M_EGT, M_BATTERY, 0, 0 };
    metric_state_t ts[8];
    bool known[8];
    for (int i = 1; i <= 5; i++) {
        metric_state_t a = alerts_state(TAG_M[i]);
        known[i] = (a != ST_INFO);
        ts[i]    = known[i] ? a : ST_OK;
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
     * nothing, and one fresh RPM says nothing about the temperatures: NOMINAL
     * needs every judged value this truck supplies (policy_summary). */
    policy_summary_t ps = policy_summary(&d, s_now, cause >= 0);
    bool ecm_fresh = (ps != SUM_NO_ECM);

    char sum[28];
    const char *txt = sum; lv_color_t col;
    switch (ps) {
    case SUM_CAN_LOST:
        txt = LV_SYMBOL_WARNING " CAN LOST"; col = C_CRIT;
        break;
    case SUM_ALARM:
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
        break;
    case SUM_NO_ECM:
        txt = "NO ECM DATA"; col = C_WARN;
        break;
    case SUM_PARTIAL:   /* no alarm among what is known — not "healthy" */
        txt = "PARTIAL DATA"; col = C_MUTED;
        break;
    default:
        txt = "NOMINAL"; col = C_OK;
        break;
    }
#if OBD_DEMO
    /* A demo image paints believable values: say so where the driver looks. */
    char dsum[40];
    lv_snprintf(dsum, sizeof dsum, "DEMO %s", txt);
    txt = dsum;
#endif
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
