#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "can_gauges.h"   /* GaugeValues_t */

/* Builds the dashboard screen and makes it active. Call once, after
 * lv_port_disp_init(). */
void ui_dashboard_create(void);

/* Push a fresh set of vehicle values to the screen. Safe to call from any
 * task -- takes the LVGL lock internally. */
void ui_dashboard_update(const GaugeValues_t *v);

#endif /* UI_DASHBOARD_H */
