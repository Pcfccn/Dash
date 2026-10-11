#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

/* Main backlight polarity, a hardware variant (see app_main.c above
 * backlight_set()): 1 = today's direct drive of the Si4599 P-FET gate from PA8,
 * 0 = the planned 2N7000 gate driver, which inverts it. The wrong value turns
 * the backlight OFF at 100 %, so the variant is also shown on the ST7735's
 * brightness overlay. Build the 2N7000 image with
 * `python tools/build.py -D BACKLIGHT_ACTIVE_LOW=0`. */
#ifndef BACKLIGHT_ACTIVE_LOW
#define BACKLIGHT_ACTIVE_LOW 1
#endif
/* Not #if: an undefined word (e.g. -D BACKLIGHT_ACTIVE_LOW=yes) would read as 0 there. */
_Static_assert(BACKLIGHT_ACTIVE_LOW == 0 || BACKLIGHT_ACTIVE_LOW == 1,
               "BACKLIGHT_ACTIVE_LOW must be 0 (2N7000 driver) or 1 (direct P-FET drive)");

/* One-time setup: LVGL/display, dashboard UI, backlight PWM, CAN gauges.
 * Call once from inside StartDefaultTask(), before its infinite loop. */
void AppMain_Init(void);

/* Call once per iteration of StartDefaultTask()'s infinite loop, in place
 * of (or alongside) its osDelay(). Drives lv_timer_handler() and pulls
 * updated CAN values into the UI. Non-blocking; caller still owns the loop
 * timing (an osDelay(5) between calls is plenty). */
void AppMain_Run(void);

/* Longest superloop period in ms over the last second (DIAG readout). */
uint32_t app_loop_max_ms(void);

#endif /* APP_MAIN_H */
