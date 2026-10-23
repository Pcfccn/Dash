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

static uint32_t tick_cb(void) { return ui_render_ms; }

static uint8_t fb[LCD_H_RES * LCD_V_RES * 2];
static uint8_t draw_buf[LCD_H_RES * LCD_V_RES * 2];

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
    ui_render_perf.state = PERF_READY;
    ui_render_perf.last_s[PF_0_100] = ui_render_perf.best_s[PF_0_100] = 11.84f;
    ui_render_perf.last_s[PF_0_60] = ui_render_perf.best_s[PF_0_60] = 5.62f;
    ui_render_perf.last_s[PF_60_100] = 6.21f; ui_render_perf.best_s[PF_60_100] = 6.02f;
    ui_render_perf.last_s[PF_QUARTER] = ui_render_perf.best_s[PF_QUARTER] = 18.37f;
    ui_render_perf.trap_kmh[PF_QUARTER] = 121.4f;
}

static const char *const NAME[UI_PAGE_COUNT] = { "1_drive", "2_trip", "3_perf", "4_diag", "5_sniff", "6_stats" };

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof draw_buf, LV_DISPLAY_RENDER_MODE_PARTIAL);

    scene();
    ui_render_ms = 100000u;
    trip_stats_reset(ui_render_ms);
    speed_cal_reset();
    cluster_ui_build();

    for (int p = 0; p < UI_PAGE_COUNT; p++) {
        cluster_ui_set_page((uint8_t)p);
        if (cluster_ui_get_page() != p) continue;        /* SNIFF refused */
        for (int i = 0; i < 100; i++) {                   /* 4 s at 25 Hz: alarm dwell, fades */
            ui_render_ms += 40u;
            cluster_ui_refresh();
            lv_timer_handler();
        }
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        char path[512];
        snprintf(path, sizeof path, "%s/%s.rgb565", dir, NAME[p]);
        FILE *f = fopen(path, "wb");
        if (!f) { perror(path); return 1; }
        fwrite(fb, 1, sizeof fb, f);
        fclose(f);
    }

    /* The same DRIVE page with alarms: hot coolant and ATF, two DTCs. */
    g_obd.cool = 113.0f; g_obd.atf = 121.0f; g_obd.dtc_count = 2;
    cluster_ui_set_page(UI_PAGE_DRIVE);
    for (int i = 0; i < 250; i++) {                       /* 10 s: past every dwell */
        ui_render_ms += 40u;
        cluster_ui_refresh();
        lv_timer_handler();
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(disp);
    char path[512];
    snprintf(path, sizeof path, "%s/7_drive_alarm.rgb565", dir);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite(fb, 1, sizeof fb, f);
    fclose(f);

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
