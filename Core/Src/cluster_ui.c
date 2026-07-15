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

/* ---- palette (from the HTML :root) --------------------------------------- */
#define C_SCREEN     lv_color_hex(0x05080c)
#define C_SURFACE    lv_color_hex(0x0b1017)
#define C_LINE       lv_color_hex(0x1b2633)
#define C_LINE_STR   lv_color_hex(0x2a394a)
#define C_TEXT       lv_color_hex(0xf4f7fa)
#define C_TEXT2      lv_color_hex(0xcad2dc)
#define C_LABEL      lv_color_hex(0x91a0b2)
#define C_MUTED      lv_color_hex(0x657386)
#define C_FAINT      lv_color_hex(0x3d4857)
#define C_OK         lv_color_hex(0x37d67a)
#define C_INFO       lv_color_hex(0x42bff5)
#define C_WARN       lv_color_hex(0xffb33e)
#define C_CRIT       lv_color_hex(0xff4f5e)
#define C_TRACK      lv_color_hex(0x1b2430)
#define C_ALERTBG    lv_color_hex(0x060a0f)
#define C_TOPBAR     lv_color_hex(0x070b10)

#define DEG "\xC2\xB0"          /* UTF-8 degree sign */

#define F12 &lv_font_montserrat_12
#define F14 &lv_font_montserrat_14
#define F20 &lv_font_montserrat_20
#define F24 &lv_font_montserrat_24
#define F28 &lv_font_montserrat_28
#define F48 &lv_font_montserrat_48

/* ---- widget handles we update in refresh() ------------------------------- */
static struct {
    lv_obj_t *page[3];

    /* alert strip */
    lv_obj_t *strip;
    lv_obj_t *tag[9];               /* MIL CLT OIL ATF EGT DPF BAT DTC CAN */
    lv_obj_t *summary;
    lv_obj_t *count;

    /* DRIVE */
    lv_obj_t *ind_mil, *can_chip;
    lv_obj_t *speed_val, *iat_val, *rpm_val, *rpm_bar;
    lv_obj_t *dm_val[4], *dm_bar[4], *dm_dot[4];   /* cool oil atf egt */
    lv_obj_t *boost_val, *boost_bar;

    /* DPF */
    lv_obj_t *soot_arc, *soot_val, *regen_chip, *regen_title, *regen_hint;
    lv_obj_t *mc_val[4];            /* egt dp since egr */

    /* DIAG */
    lv_obj_t *dtc_chip, *mil_text, *dtc_msg;
    lv_obj_t *st_val[4];            /* batt iat load rail */
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

/* enhanced (mode-22) metrics have no DID mapped yet -> treat as "no data" */
static bool is_enhanced(metric_key_t k)
{
    return k == M_ATF || k == M_SOOT || k == M_DPF_DP ||
           k == M_SINCE_REGEN || k == M_EGR_T;
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
        case M_RAIL:  return d->rail;    default: return 0;
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

/* pill chip (rounded, bordered, small caps) */
static lv_obj_t *mk_chip(lv_obj_t *p, const char *txt)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_size(c, LV_SIZE_CONTENT, 18);
    lv_obj_set_style_bg_color(c, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, C_LINE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 9, 0);
    lv_obj_set_style_pad_hor(c, 7, 0);
    lv_obj_set_style_pad_ver(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = mk_label(c, txt, F12, C_MUTED);
    lv_obj_center(l);
    return c;
}
static void chip_set(lv_obj_t *chip, const char *txt, lv_color_t col)
{
    lv_obj_t *l = lv_obj_get_child(chip, 0);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_border_color(chip, col, 0);
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

/* topbar strip (title centered, small indicators/chips at the sides) */
static lv_obj_t *mk_topbar(lv_obj_t *page, const char *title)
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
    lv_obj_t *t = mk_label(tb, title, F12, C_TEXT2);
    lv_obj_center(t);
    return tb;
}

/* ------------------------------------------------------------------ page: DRIVE */
static void build_drive(void)
{
    lv_obj_t *pg = ui.page[0];

    lv_obj_t *tb = mk_topbar(pg, "DRIVE");
    ui.ind_mil = mk_label(tb, "MIL", F12, C_FAINT);
    lv_obj_align(ui.ind_mil, LV_ALIGN_LEFT_MID, 8, 0);
    ui.can_chip = mk_chip(tb, "CAN");
    lv_obj_align(ui.can_chip, LV_ALIGN_RIGHT_MID, -6, 0);

    /* hero: gear | speed(+IAT) | rpm */
    lv_obj_t *hero = lv_obj_create(pg);
    lv_obj_set_pos(hero, 0, 28);
    lv_obj_set_size(hero, 320, 108);
    plain(hero);
    lv_obj_set_style_border_color(hero, C_LINE, 0);
    lv_obj_set_style_border_width(hero, 1, 0);
    lv_obj_set_style_border_side(hero, LV_BORDER_SIDE_BOTTOM, 0);

    lv_obj_t *gear = mk_card(hero, 8, 8, 92, 92);
    lv_obj_t *gl = mk_label(gear, "GEAR", F12, C_MUTED);
    lv_obj_align(gl, LV_ALIGN_TOP_MID, 0, -2);
    lv_obj_t *gv = mk_label(gear, "--", F28, C_TEXT2);
    lv_obj_align(gv, LV_ALIGN_CENTER, 0, 8);

    /* speed panel */
    ui.speed_val = mk_label(hero, "--", F48, C_TEXT);
    lv_obj_set_width(ui.speed_val, 130);
    lv_obj_set_style_text_align(ui.speed_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui.speed_val, 105, 34);
    lv_obj_t *su = mk_label(hero, "KM/H", F12, C_LABEL);
    lv_obj_set_width(su, 130);
    lv_obj_set_style_text_align(su, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(su, 105, 88);
    lv_obj_t *ia = mk_label(hero, "IAT", F12, C_MUTED);
    lv_obj_set_pos(ia, 118, 104);
    ui.iat_val = mk_label(hero, "--" DEG, F14, C_TEXT2);
    lv_obj_set_pos(ui.iat_val, 145, 102);

    /* rpm panel */
    lv_obj_t *rp = mk_card(hero, 242, 8, 70, 92);
    lv_obj_set_style_pad_all(rp, 5, 0);
    lv_obj_t *ru = mk_label(rp, "RPM", F12, C_MUTED);
    lv_obj_align(ru, LV_ALIGN_TOP_MID, 0, -1);
    ui.rpm_val = mk_label(rp, "--", F24, C_TEXT2);
    lv_obj_align(ui.rpm_val, LV_ALIGN_CENTER, 0, -2);
    ui.rpm_bar = mk_bar(rp, 0, 62, 56, 3);

    /* 2x2 metric grid on a 1px line background */
    lv_obj_t *grid = lv_obj_create(pg);
    lv_obj_set_pos(grid, 0, 136);
    lv_obj_set_size(grid, 320, 222);
    lv_obj_set_style_bg_color(grid, C_LINE, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static const char *NAMES[4] = { "COOLANT", "OIL", "ATF", "EGT" };
    const int cx[4] = { 0, 160, 0, 160 };
    const int cy[4] = { 0, 0, 111, 111 };
    const int cw[4] = { 159, 160, 159, 160 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *cell = lv_obj_create(grid);
        lv_obj_set_pos(cell, cx[i], cy[i]);
        lv_obj_set_size(cell, cw[i], 110);
        lv_obj_set_style_bg_color(cell, C_SCREEN, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell, 0, 0);
        lv_obj_set_style_radius(cell, 0, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *nm = mk_label(cell, NAMES[i], F12, C_LABEL);
        lv_obj_set_pos(nm, 12, 10);
        ui.dm_dot[i] = mk_dot(cell, cw[i] - 16, 12);

        ui.dm_val[i] = mk_label(cell, "--", F48, C_TEXT);
        lv_obj_set_width(ui.dm_val[i], cw[i]);
        lv_obj_set_style_text_align(ui.dm_val[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(ui.dm_val[i], 0, 30);

        ui.dm_bar[i] = mk_bar(cell, 12, 86, cw[i] - 24, 6);

        /* warn/crit tick markers over the gauge, positioned from thresholds */
        const metric_cfg_t *mc = &metrics[DRIVE_M[i]];
        if (mc->kind == THR_HIGH_ONLY) {
            int bw = cw[i] - 24;
            float span = mc->scale_max - mc->scale_min;
            int wx = 12 + (int)((mc->warn_high - mc->scale_min) * bw / span);
            int cxk = 12 + (int)((mc->crit_high - mc->scale_min) * bw / span);
            mk_tick(cell, wx, 83, 2, 12, C_WARN);
            mk_tick(cell, cxk, 83, 2, 12, C_CRIT);
        }

        char lo[8], hi[8];
        fmt(lo, sizeof lo, metrics[DRIVE_M[i]].scale_min, 0);
        fmt(hi, sizeof hi, metrics[DRIVE_M[i]].scale_max, 0);
        lv_obj_t *tl = mk_label(cell, lo, F12, C_FAINT);
        lv_obj_set_pos(tl, 12, 95);
        lv_obj_t *th = mk_label(cell, hi, F12, C_FAINT);
        lv_obj_align(th, LV_ALIGN_TOP_RIGHT, -12, 95);
    }

    /* boost strip */
    lv_obj_t *bs = lv_obj_create(pg);
    lv_obj_set_pos(bs, 0, 358);
    lv_obj_set_size(bs, 320, 66);
    lv_obj_set_style_bg_color(bs, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(bs, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bs, C_LINE, 0);
    lv_obj_set_style_border_width(bs, 1, 0);
    lv_obj_set_style_border_side(bs, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(bs, 0, 0);
    lv_obj_set_style_pad_all(bs, 0, 0);
    lv_obj_clear_flag(bs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *bl = mk_label(bs, "BOOST", F14, C_LABEL);
    lv_obj_set_pos(bl, 12, 10);
    ui.boost_val = mk_label(bs, "--", F28, C_TEXT);
    lv_obj_align(ui.boost_val, LV_ALIGN_TOP_RIGHT, -40, 6);
    lv_obj_t *bu = mk_label(bs, "bar", F14, C_LABEL);
    lv_obj_align(bu, LV_ALIGN_TOP_RIGHT, -12, 14);
    ui.boost_bar = mk_bar(bs, 12, 44, 296, 5);

    mk_pager(pg, 0);
}

/* --------------------------------------------------------------- page: DPF */
static void build_dpf(void)
{
    lv_obj_t *pg = ui.page[1];

    lv_obj_t *tb = mk_topbar(pg, "DPF");
    ui.regen_chip = mk_chip(tb, "IDLE");
    lv_obj_align(ui.regen_chip, LV_ALIGN_RIGHT_MID, -6, 0);

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

    ui.soot_val = mk_label(ui.soot_arc, "--", F48, C_TEXT);
    lv_obj_align(ui.soot_val, LV_ALIGN_CENTER, 0, -4);
    lv_obj_t *sl = mk_label(ui.soot_arc, "SOOT", F12, C_MUTED);
    lv_obj_align(sl, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *ml = mk_label(hero, "REGENERATION", F12, C_MUTED);
    lv_obj_set_pos(ml, 150, 34);
    ui.regen_title = mk_label(hero, "Inactive", F20, C_TEXT);
    lv_obj_set_pos(ui.regen_title, 150, 52);
    ui.regen_hint = mk_label(hero, "No regen request\nfrom ECM.", F14, C_LABEL);
    lv_obj_set_pos(ui.regen_hint, 150, 84);
    mk_tick(hero, 150, 134, 13, 3, C_WARN);
    lv_obj_t *tn1 = mk_label(hero, "70% warning", F12, C_MUTED);
    lv_obj_set_pos(tn1, 168, 128);
    mk_tick(hero, 150, 152, 13, 3, C_CRIT);
    lv_obj_t *tn2 = mk_label(hero, "85% regen", F12, C_MUTED);
    lv_obj_set_pos(tn2, 168, 146);

    /* 2x2 mini-cards */
    static const char *ML[4] = { "EGT", "dP DPF", "SINCE REGEN", "EGR" };
    static const char *MU[4] = { DEG "C", "kPa", "km", DEG "C" };
    const int mx[4] = { 10, 165, 10, 165 };
    const int my[4] = { 212, 212, 282, 282 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = mk_card(pg, mx[i], my[i], 145, 62);
        lv_obj_t *nm = mk_label(c, ML[i], F12, C_LABEL);
        lv_obj_set_pos(nm, 0, 0);
        ui.mc_val[i] = mk_label(c, "--", F28, C_TEXT);
        lv_obj_set_pos(ui.mc_val[i], 0, 13);
        lv_obj_t *u = mk_label(c, MU[i], F14, C_LABEL);
        lv_obj_align(u, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    mk_pager(pg, 1);
}

/* -------------------------------------------------------------- page: DIAG */
static void build_diag(void)
{
    lv_obj_t *pg = ui.page[2];

    lv_obj_t *tb = mk_topbar(pg, "DIAGNOSTICS");
    ui.dtc_chip = mk_chip(tb, "0 DTC");
    lv_obj_align(ui.dtc_chip, LV_ALIGN_RIGHT_MID, -6, 0);

    /* MIL summary row */
    lv_obj_t *mil = mk_card(pg, 10, 38, 300, 42);
    lv_obj_set_style_pad_all(mil, 8, 0);
    lv_obj_t *ml = mk_label(mil, "MIL / CHECK ENGINE", F12, C_MUTED);
    lv_obj_set_pos(ml, 0, 0);
    ui.mil_text = mk_label(mil, "--", F14, C_TEXT2);
    lv_obj_set_pos(ui.mil_text, 0, 16);

    /* DTC message area */
    lv_obj_t *box = lv_obj_create(pg);
    lv_obj_set_pos(box, 10, 88);
    lv_obj_set_size(box, 300, 96);
    lv_obj_set_style_bg_color(box, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_50, 0);
    lv_obj_set_style_border_color(box, C_LINE_STR, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    ui.dtc_msg = mk_label(box, "NO STORED CODES", F14, C_TEXT2);
    lv_obj_set_style_text_align(ui.dtc_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(ui.dtc_msg);

    /* 2x2 stat cards */
    static const char *SL[4] = { "BATT", "IAT", "LOAD", "RAIL" };
    static const char *SU[4] = { "V", DEG "C", "%", "MPa" };
    const int sx[4] = { 10, 165, 10, 165 };
    const int sy[4] = { 192, 192, 256, 256 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = mk_card(pg, sx[i], sy[i], 145, 58);
        lv_obj_t *nm = mk_label(c, SL[i], F12, C_MUTED);
        lv_obj_set_pos(nm, 0, 0);
        ui.st_val[i] = mk_label(c, "--", F24, C_TEXT);
        lv_obj_set_pos(ui.st_val[i], 0, 14);
        lv_obj_t *u = mk_label(c, SU[i], F12, C_LABEL);
        lv_obj_align(u, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

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
        lv_obj_align(ui.tag[i], LV_ALIGN_LEFT_MID, 4 + i * 24, 0);
    }
    ui.count = mk_label(ui.strip, "", F12, C_CRIT);
    lv_obj_align(ui.count, LV_ALIGN_RIGHT_MID, -8, 0);
    ui.summary = mk_label(ui.strip, "ALL OK", F12, C_OK);
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
    bool ok = live && !is_enhanced(k);
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
    float v = mval(d, k);
    char b[16];
    fmt(b, sizeof b, v, metrics[k].decimals);
    lv_label_set_text(val, b);

    metric_state_t s = metric_state(k, v);
    lv_color_t c = state_color(s);
    lv_obj_set_style_text_color(val, (s >= ST_WARN) ? c : C_TEXT, 0);
    if (dot) lv_obj_set_style_bg_color(dot, c, 0);
    if (bar) {
        lv_bar_set_value(bar, pct_of(k, v), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, (s >= ST_WARN) ? c : C_INFO, LV_PART_INDICATOR);
    }
}

static void set_tag(lv_obj_t *tag, metric_state_t s)
{
    lv_color_t c = (s == ST_CRIT) ? C_CRIT : (s == ST_WARN) ? C_WARN : C_FAINT;
    lv_obj_set_style_text_color(tag, c, 0);
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
    d.mil = g_obd.mil; d.dtc_count = g_obd.dtc_count; d.can_ok = g_obd.can_ok;
    bool live = d.can_ok;

    char b[16], nb[12];

    /* ----- DRIVE ----- */
    if (live) { fmt(b, sizeof b, d.speed, 0); lv_label_set_text(ui.speed_val, b); }
    else        lv_label_set_text(ui.speed_val, "--");
    if (live) { fmt(nb, sizeof nb, d.iat, 0); lv_snprintf(b, sizeof b, "%s" DEG, nb); lv_label_set_text(ui.iat_val, b); }
    else        lv_label_set_text(ui.iat_val, "--" DEG);
    if (live) { fmt(b, sizeof b, d.rpm, 0); lv_label_set_text(ui.rpm_val, b); }
    else        lv_label_set_text(ui.rpm_val, "--");
    lv_bar_set_value(ui.rpm_bar, live ? pct_of(M_RPM, d.rpm) : 0, LV_ANIM_OFF);
    {
        metric_state_t rs = live ? metric_state(M_RPM, d.rpm) : ST_OK;
        lv_obj_set_style_bg_color(ui.rpm_bar, (rs >= ST_WARN) ? state_color(rs) : C_INFO, LV_PART_INDICATOR);
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.dm_val[i], ui.dm_bar[i], ui.dm_dot[i], DRIVE_M[i], &d, live);
    set_metric(ui.boost_val, ui.boost_bar, NULL, M_BOOST, &d, live);

    lv_obj_set_style_text_color(ui.ind_mil, d.mil ? C_CRIT : C_FAINT, 0);
    chip_set(ui.can_chip, "CAN", live ? C_OK : C_CRIT);

    /* ----- DPF ----- */
    set_metric(ui.soot_val, NULL, NULL, M_SOOT, &d, live);
    {
        bool ok = live && !is_enhanced(M_SOOT);
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
        lv_snprintf(b, sizeof b, "%u DTC", (unsigned)d.dtc_count);
        chip_set(ui.dtc_chip, b, C_WARN);
        lv_snprintf(mb, sizeof mb, LV_SYMBOL_WARNING "  %u STORED CODE%s",
                    (unsigned)d.dtc_count, d.dtc_count == 1 ? "" : "S");
        lv_label_set_text(ui.dtc_msg, mb);
        lv_obj_set_style_text_color(ui.dtc_msg, C_WARN, 0);
    } else {
        chip_set(ui.dtc_chip, "0 DTC", C_OK);
        if (live) {
            lv_label_set_text(ui.dtc_msg, LV_SYMBOL_OK "  NO STORED CODES");
            lv_obj_set_style_text_color(ui.dtc_msg, C_OK, 0);
        } else {
            lv_label_set_text(ui.dtc_msg, "NO DATA");
            lv_obj_set_style_text_color(ui.dtc_msg, C_MUTED, 0);
        }
    }
    for (int i = 0; i < 4; i++)
        set_metric(ui.st_val[i], NULL, NULL, STAT_M[i], &d, live);

    /* ----- alert strip ----- */
    metric_state_t ts[9];
    ts[0] = d.mil ? ST_CRIT : ST_OK;                        /* MIL */
    ts[1] = live ? metric_state(M_COOL, d.cool) : ST_OK;    /* CLT */
    ts[2] = live ? metric_state(M_OIL, d.oil) : ST_OK;      /* OIL */
    ts[3] = ST_OK;                                          /* ATF (no data) */
    ts[4] = live ? metric_state(M_EGT, d.egt) : ST_OK;      /* EGT */
    ts[5] = ST_OK;                                          /* DPF (no data) */
    ts[6] = live ? metric_state(M_BATTERY, d.battery):ST_OK;/* BAT */
    ts[7] = d.dtc_count > 0 ? ST_WARN : ST_OK;              /* DTC */
    ts[8] = live ? ST_OK : ST_CRIT;                         /* CAN */

    int worst = ST_OK, nalarm = 0;
    for (int i = 0; i < 9; i++) {
        set_tag(ui.tag[i], ts[i]);
        if (ts[i] > worst) worst = ts[i];
        if (ts[i] >= ST_WARN) nalarm++;
    }

    const char *txt; lv_color_t col;
    if (!live)                 { txt = LV_SYMBOL_WARNING " CAN LOST"; col = C_CRIT; }
    else if (worst == ST_CRIT) { txt = LV_SYMBOL_WARNING " CHECK";    col = C_CRIT; }
    else if (worst == ST_WARN) { txt = LV_SYMBOL_WARNING " WARN";     col = C_WARN; }
    else                       { txt = LV_SYMBOL_OK " ALL OK";        col = C_OK;   }
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
