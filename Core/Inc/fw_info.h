#ifndef FW_INFO_H
#define FW_INFO_H

/* Which image this is, for the DIAG page and the ST7735 start-up line.
 *
 * FW_GIT / FW_GIT_DIRTY come from gen/fw_version.h, written by tools/build.py
 * from git (dirty = firmware sources changed since that commit). A CubeIDE
 * build has no such file and says "IDE".
 *
 * The variant is derived here from the build flags, so an image built for the
 * bench, or for the other backlight driver, says so on screen. That matters
 * most for the 2N7000 board: the default image turns its main backlight off,
 * and only the small screen is left to tell which image went in. */

#if defined(__has_include)
#if __has_include("fw_version.h")
#include "fw_version.h"
#endif
#endif
#ifndef FW_GIT
#define FW_GIT       "IDE"
#define FW_GIT_DIRTY 0
#endif

#include "app_main.h"        /* BACKLIGHT_ACTIVE_LOW            */
#include "cluster_ui.h"      /* UI_PALETTE_TN                   */
#include "lv_port_disp.h"    /* LCD_FLIP                        */
#include "cluster_config.h"  /* OBD_DEMO, OILP_DPID_ENABLE, OILP_VALIDATED */
#include "cluster_policy.h"  /* SNIFF_BENCH_OVERRIDE            */
#include "fault.h"           /* FAULT_TEST                       */

#define FW_STR_(x) #x
#define FW_STR(x)  FW_STR_(x)

#if OBD_DEMO
#define FW_V_DEMO " DEMO"        /* synthetic values, no CAN          */
#else
#define FW_V_DEMO ""
#endif
#if FAULT_TEST
#define FW_V_FT " FT" FW_STR(FAULT_TEST)
#else
#define FW_V_FT ""
#endif
#if SNIFF_BENCH_OVERRIDE
#define FW_V_SNIFF " SNIFF"      /* SNIFF opens without a selector    */
#else
#define FW_V_SNIFF ""
#endif
#if OILP_DPID_ENABLE
#define FW_V_OILP " KOEO"        /* $2C / $AA oil-pressure experiment */
#else
#define FW_V_OILP ""
#endif
#if OILP_VALIDATED
#define FW_V_OILPV " OILPV"      /* oil-pressure alarms on (validated) */
#else
#define FW_V_OILPV ""
#endif
#if UI_PALETTE_TN
#define FW_V_PAL ""
#else
#define FW_V_PAL " PAL0"         /* previous colour palette           */
#endif
#if LCD_FLIP
#define FW_V_FLIP " FLIP"        /* main panel turned 180 degrees     */
#else
#define FW_V_FLIP ""
#endif
#if LCD_PROBE
#define FW_V_PROBE " PROBE"      /* 16-bit controller test at start   */
#else
#define FW_V_PROBE ""
#endif
#if BACKLIGHT_ACTIVE_LOW
#define FW_V_BL ""
#else
#define FW_V_BL " INV"           /* 2N7000 backlight driver           */
#endif

/* 1 = not an image to drive with: fake data, fault injection, a relaxed
 * SNIFF gate, or writes to ECM RAM. */
#define FW_BENCH (OBD_DEMO || FAULT_TEST || SNIFF_BENCH_OVERRIDE || OILP_DPID_ENABLE)
#if FW_BENCH
#define FW_KIND "BENCH"
#else
#define FW_KIND "ROAD"
#endif

#define FW_VARIANT       FW_KIND FW_V_DEMO FW_V_FT FW_V_SNIFF FW_V_OILP FW_V_OILPV FW_V_PAL FW_V_FLIP FW_V_PROBE FW_V_BL
#define FW_VARIANT_SHORT FW_KIND FW_V_BL          /* fits the 80 px ST7735 */

#if FW_GIT_DIRTY
#define FW_ID FW_GIT "+"
#else
#define FW_ID FW_GIT
#endif

#endif /* FW_INFO_H */
