/* ============================================================================
 *  cluster_app.h — glue for a project where the DISPLAY IS ALREADY SET UP.
 *
 *  This package provides only the SCREEN DESIGN and the DATA SOURCES.
 *  It assumes your project already:
 *    - called lv_init() and registered a working LVGL display driver,
 *    - calls lv_timer_handler() in its own loop.
 *
 *  You add, in your existing code:
 *     cluster_app_init(&hfdcan1);   // after your lv_init + display setup
 *     cluster_app_run();            // once per loop iteration (non-blocking)
 * ========================================================================== */
#ifndef CLUSTER_APP_H
#define CLUSTER_APP_H

#include "stm32h7xx_hal.h"

/* Builds the 3 cluster screens on lv_scr_act() and starts the OBD poller.
 * Pass the FDCAN handle you use for the vehicle bus. */
void cluster_app_init(FDCAN_HandleTypeDef *hfdcan);

/* Non-blocking. Call once per loop iteration. Drives the OBD request
 * scheduler, the UI refresh, and the CAN watchdog. Does NOT call
 * lv_timer_handler() — your project already does that. */
void cluster_app_run(void);

#endif /* CLUSTER_APP_H */
