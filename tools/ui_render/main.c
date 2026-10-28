/* tools/ui_render.py: build the real cluster UI on the PC, fill it with one
 * scene, and dump every page as raw RGB565 (320x480, little endian) into the
 * directory given as argv[1]. The script turns them into PNGs. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "cluster_ui.h"
#include "lv_port_disp.h"      /* LCD_H_RES, LCD_V_RES */
#include "fdcan_obd.h"
#include "gps.h"
#include "odo.h"
#include "perf.h"
#include "trip_stats.h"
#include "speed_cal.h"

extern uint32_t    ui_render_ms;
extern gps_state_t ui_render_gps;
extern odo_t       ui_render_odo;
extern perf_t      ui_render_perf;
extern obd_health_t ui_render_health;

static uint32_t tick_cb(void) { return ui_render_ms; }

static uint8_t fb[LCD_H_RES * LCD_V_RES * 2];
static uint8_t draw_buf[LCD_H_RES * LCD_V_RES * 2];
static lv_display_t *disp;
static const char *dir = ".";

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int32_t w = lv_area_get_width(a);
    for (int32_t y = a->y1; y <= a->y2; y++)
        memcpy(&fb[(y * LCD_H_RES + a->x1) * 2], px + (y - a->y1) * w * 2, (size_t)w * 2);
    lv_display_flush_ready(d);
}

/* Cruising in D: warm engine, everything this truck answers is present
 * (oil temp from DID 0x1154); EGT and oil pressure are NaN as in the truck. */
static void scene(void)
{
    volatile obd_data_t *d = &g_obd;
    d->speed = 87.0f;  d->rpm = 1850.0f; d->cool = 88.0f;  d->oil = 96.0f;
    d->iat = 31.0f;    d->load = 42.0f;  d->boost = 0.8f;  d->rail = 650.0f;
    d->egt = NAN;      d->battery = 14.1f; d->atf = 72.0f; d->oil_press = NAN;
    d->gear = 5;       d->sel_range = 4; d->mil = false;   d->dtc_count = 0;
    d->mil_valid = true; d->can_ok = true;

    gps_state_t *g = &ui_render_gps;
    memset(g, 0, sizeof *g);
    g->link = GPS_LINK_RUN; g->baud = 115200; g->rate_hz = 10;
    g->fix.rmc_fix = true; g->fix.speed_kmh = 86.4f; g->fix.quality = 1;
    g->fix.sats = 9; g->fix.hdop = 0.9f; g->fix.alt_m = 112.0f;
    g->fix.lat_deg = -33.87; g->fix.lon_deg = 151.21;
    g->fix.hh = 3; g->fix.mm = 25; g->fix.ss = 41;
    g->fix.day = 23; g->fix.month = 10; g->fix.year = 2026;

    odo_init(&ui_render_odo, 184237000u, 412300u, 5108400u, 16380u, 0u);
    perf_init(&ui_render_perf);
    ui_render_perf.state = PERF_MOVING;           /* cruising: "STOP TO ARM" */
    ui_render_perf.last_s[PF_0_100] = ui_render_perf.best_s[PF_0_100] = 11.84f;
    ui_render_perf.last_s[PF_0_60] = ui_render_perf.best_s[PF_0_60] = 5.62f;
    ui_render_perf.last_s[PF_60_100] = 6.21f; ui_render_perf.best_s[PF_60_100] = 6.02f;
    ui_render_perf.last_s[PF_QUARTER] = ui_render_perf.best_s[PF_QUARTER] = 18.37f;
    ui_render_perf.trap_kmh[PF_QUARTER] = 121.4f;
}

/* Advance the UI clock at 25 Hz, with the GPS feeding the odometer like the
 * firmware (so TRIP says SPEED FROM GPS), every value stamped now. */
static void run(int iterations)
{
    for (int i = 0; i < iterations; i++) {
        ui_render_ms += 40u;
        for (int k = 0; k < M_COUNT; k++) g_obd.upd_ms[k] = ui_render_ms;
        g_obd.sel_upd_ms = g_obd.mil_upd_ms = ui_render_ms;
        if (ui_render_gps.fix.rmc_fix) {
            ui_render_gps.fix_ms = ui_render_gps.rmc_ms = ui_render_gps.gga_ms = ui_render_ms;
            odo_gps(&ui_render_odo, ui_render_gps.fix.speed_kmh, true, ui_render_ms);
        }
        cluster_ui_refresh();
        lv_timer_handler();
    }
}

static int dump(const char *name)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(disp);
    char path[512];
    snprintf(path, sizeof path, "%s/%s.rgb565", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite(fb, 1, sizeof fb, f);
    fclose(f);
    return 0;
}

static const char *const NAME[UI_PAGE_COUNT] = { "1_drive", "2_trip", "3_perf", "4_diag", "5_sniff", "6_stats" };

int main(int argc, char **argv)
{
    if (argc > 1) dir = argv[1];
    lv_init();
    lv_tick_set_cb(tick_cb);
    disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof draw_buf, LV_DISPLAY_RENDER_MODE_PARTIAL);

    scene();
    ui_render_ms = 100000u;
    trip_stats_reset(ui_render_ms);
    speed_cal_reset();
    cluster_ui_build();

    for (int p = 0; p < UI_PAGE_COUNT; p++) {
        if (p == UI_PAGE_SNIFF) continue;                 /* parked scene below */
        cluster_ui_set_page((uint8_t)p);
        run(100);                                         /* 4 s: alarm dwell, fades */
        if (dump(NAME[p])) return 1;
    }

    /* DIAG line 3 rotates every 4 s through five views (cluster_ui.c):
     * oil probe, maps 00/20/40, maps 60/80/A0 + 5E, DPF values, DPF counts. */
    cluster_ui_set_page(UI_PAGE_DIAG);
    for (int view = 0; view <= 4; view++) {
        ui_render_ms = (ui_render_ms / 20000u + 1u) * 20000u + (uint32_t)view * 4000u + 100u;
        run(1);
        char vn[32];
        snprintf(vn, sizeof vn, "4_diag_view%d", view);
        if (dump(vn)) return 1;
    }

    /* SNIFF opens only parked: fresh selector in P, no fresh speed above
     * 3 km/h (policy_sniff_allowed). */
    g_obd.sel_range = 1; g_obd.speed = 0.0f; g_obd.rpm = 760.0f;
    ui_render_gps.fix.speed_kmh = 0.0f;
    run(1);
    cluster_ui_set_page(UI_PAGE_SNIFF);
    if (cluster_ui_get_page() == UI_PAGE_SNIFF) {
        run(25);
        if (dump(NAME[UI_PAGE_SNIFF])) return 1;
    } else {
        printf("SNIFF refused by the policy in the parked scene: not rendered\n");
    }

    /* The same DRIVE page with alarms: hot coolant and ATF, two DTCs. */
    scene();
    g_obd.cool = 113.0f; g_obd.atf = 121.0f; g_obd.dtc_count = 2;
    cluster_ui_set_page(UI_PAGE_DRIVE);
    run(250);                                             /* 10 s: past every dwell */
    if (dump("7_drive_alarm")) return 1;

    /* DIAG line 1 at about its longest: NRC, LOOP, Q, CAP, TXL, BOFF after
     * two recovered bus-offs; then after the fifth in a row, when the node
     * only listens until the next power-up (fdcan_obd.c, review C9). */
    ui_render_health.busoff_recover = 2u;
    ui_render_health.tx_lost = 160u; ui_render_health.txn_capped = 1u;
    ui_render_health.rx_hwm = 37u;
    g_obd.last_nrc_sid = 0x22; g_obd.last_nrc = 0x31;
    cluster_ui_set_page(UI_PAGE_DIAG);
    run(1);
    if (dump("8_diag_counters")) return 1;
    ui_render_health.busoff_recover = 5u; ui_render_health.tx_off = true;
    run(1);
    if (dump("8_diag_txoff")) return 1;

    /* Every page has been built and drawn: what the LVGL heap holds now,
     * and its peak (render-time layers included). */
    lv_mem_monitor_t mm;
    lv_mem_monitor(&mm);
    printf("LVGL heap (%u-bit build): %lu bytes, used %lu (%u %%), peak %lu (%u %%), frag %u %%\n",
           (unsigned)(sizeof(void *) * 8u), (unsigned long)mm.total_size,
           (unsigned long)(mm.total_size - mm.free_size), (unsigned)mm.used_pct,
           (unsigned long)mm.max_used,
           (unsigned)(mm.total_size ? (mm.max_used * 100u + mm.total_size - 1u) / mm.total_size : 0u),
           (unsigned)mm.frag_pct);
    return 0;
}
