/* ============================================================================
 *  cluster_app.c — glue for a project with an already-working display.
 *  No lv_init(), no panel driver here: the project owns those.
 * ========================================================================== */
#include "cluster_app.h"
#include "fdcan_obd.h"
#include "cluster_ui.h"
#include "cluster_config.h"   /* OBD_DEMO */
#include "can_sniff.h"        /* can_sniff_service */
#include "gps.h"              /* GPS fixes for the speedometer check */
#include "speed_cal.h"
#include "trip_stats.h"

void cluster_app_init(FDCAN_HandleTypeDef *hfdcan) {
    trip_stats_reset(HAL_GetTick());   /* STATS page: counts since power-up */
    speed_cal_reset();
    cluster_ui_build();         /* create the pages + alert strip on the active screen */
    obd_init(hfdcan);           /* FDCAN filters + start; RX by interrupt into a ring    */
}

/* Every valid decode (fdcan_obd.c), with the frame's receive time: interval
 * and range statistics, and the OBD side of the speedometer check. */
void obd_on_stamp(uint8_t ch, float v, uint32_t rx_ms) {
    trip_stats_update(ch, v, rx_ms);
    if (ch == (uint8_t)M_SPEED) speed_cal_obd(v, rx_ms);
}

/* The GPS side: each new fix, with its receive time. Before obd_rx_poll(),
 * so the OBD speeds decoded below find the GPS samples of the same period. */
static void feed_gps(void) {
    static uint32_t last_fix;
    const gps_state_t *g = gps_state();
    if (g->fix_ms == 0u || g->fix_ms == last_fix) return;
    last_fix = g->fix_ms;
    speed_cal_gps(g->fix.speed_kmh, g->fix.rmc_fix, g->fix.sats, g->fix.hdop, g->fix_ms);
}

void cluster_app_run(void) {
    static uint32_t t_ui = 0;
    uint32_t now = HAL_GetTick();

    feed_gps();

#if OBD_DEMO
    obd_demo_tick();                                                   /* synthetic values */
#else
    static uint32_t t_poll = 0, t_wdg = 0;
    obd_rx_poll();                                                     /* drain RX every call */
    can_sniff_service();                     /* retry a refused narrow-filter restore */
    if (now - t_poll >= 25)   { t_poll = now; obd_poll_tick(); }        /* ~40 Hz requests */
    if (now - t_wdg  >= 1000) { t_wdg  = now; obd_watchdog_tick_1hz(); }
#endif

    if (now - t_ui   >= 40)   { t_ui   = now; cluster_ui_refresh(); }   /* ~25 Hz UI       */
}
