/* ============================================================================
 *  cluster_app.c — glue for a project with an already-working display.
 *  No lv_init(), no panel driver here: the project owns those.
 * ========================================================================== */
#include "cluster_app.h"
#include "fdcan_obd.h"
#include "cluster_ui.h"
#include "cluster_config.h"   /* OBD_DEMO */

void cluster_app_init(FDCAN_HandleTypeDef *hfdcan) {
    cluster_ui_build();         /* create the 3 pages + alert strip on the active screen */
    obd_init(hfdcan);           /* FDCAN filters + start (RX is polled below)            */
}

void cluster_app_run(void) {
    static uint32_t t_ui = 0;
    uint32_t now = HAL_GetTick();

#if OBD_DEMO
    obd_demo_tick();                                                   /* synthetic values */
#else
    static uint32_t t_poll = 0, t_wdg = 0;
    obd_rx_poll();                                                     /* drain RX every call */
    if (now - t_poll >= 25)   { t_poll = now; obd_poll_tick(); }        /* ~40 Hz requests */
    if (now - t_wdg  >= 1000) { t_wdg  = now; obd_watchdog_tick_1hz(); }
#endif

    if (now - t_ui   >= 40)   { t_ui   = now; cluster_ui_refresh(); }   /* ~25 Hz UI       */
}
