/* LVGL 9.3 configuration for InstrumentCluster.
 *
 * Placement matters: this file must live at Middlewares/lv_conf.h (sibling
 * of the Middlewares/lvgl folder), NOT in Core/Inc. LVGL's
 * lv_conf_internal.h finds it via a relative include ("../../lv_conf.h"
 * from inside lvgl/src/), so no extra include path is needed for it.
 *
 * Tick note: LVGL 9 removed the LV_TICK_CUSTOM config option. The tick is
 * wired up in code instead — lv_port_disp_init() calls
 * lv_tick_set_cb(osKernelGetTickCount).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*------------- Color -------------*/
#define LV_COLOR_DEPTH 16      /* LVGL renders RGB565; lv_port_disp.c expands to the panel's 18bpp wire format on flush */

/*------------- Memory -------------*/
#define LV_MEM_SIZE (128 * 1024U)  /* LVGL's internal heap; 3-page cluster + render buffers */

/*------------- OS integration -------------*/
/* Render synchronously inside lv_timer_handler (called from the one UI task).
 * NOT LV_OS_FREERTOS: that makes LVGL dispatch drawing to a separate worker
 * thread, which deadlocked here (the dispatcher waited forever for a worker
 * that never ran). We update the UI from a single task, so no threading is
 * needed; lv_lock()/lv_unlock() become harmless no-ops. */
#define LV_USE_OS LV_OS_NONE

/*------------- Rendering -------------*/
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_DRAW_SW_COMPLEX 1

/*------------- LCD SPI controller drivers -------------*/
/* Small on-board 0.96" ST7735 (160x80) used as a status display, see
 * st7735_status.c. Enabling this auto-enables LV_USE_GENERIC_MIPI. */
#define LV_USE_ST7735 1

/*------------- Logging (cheap to leave on for bring-up) -------------*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

/*------------- Widgets used by ui_dashboard.c -------------*/
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_LABEL 1
#define LV_USE_SCALE 1
#define LV_USE_LINE 1

/* Disable heavier widgets we don't use to keep flash/RAM down */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LIST 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/*------------- Fonts -------------*/
#define LV_FONT_MONTSERRAT_12 1   /* micro uppercase labels in the cluster */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1   /* used by the 3-page Colorado cluster */
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1   /* used by the 3-page Colorado cluster */
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*------------- Misc -------------*/
#define LV_DPI_DEF 130

#endif /* LV_CONF_H */
