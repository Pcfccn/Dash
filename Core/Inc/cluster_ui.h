/* ============================================================================
 *  cluster_ui.h — LVGL UI for the Colorado 2.8 cluster (3 pages + alert strip)
 *  Build once with cluster_ui_build(); refresh from the main loop.
 * ========================================================================== */
#ifndef CLUSTER_UI_H
#define CLUSTER_UI_H

#include <stdint.h>

void cluster_ui_build(void);        /* create all widgets (call after lv_init) */
void cluster_ui_refresh(void);      /* push g_obd -> widgets (main ctx, ~20Hz) */
void cluster_ui_set_page(uint8_t p);/* 0=DRIVE 1=DPF 2=DIAG                     */
void cluster_ui_next_page(void);    /* wheel/EXTI button -> cycle pages         */
uint8_t cluster_ui_get_page(void);  /* current page index (for diagnostics)     */

#endif /* CLUSTER_UI_H */
