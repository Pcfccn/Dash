#ifndef APP_MAIN_H
#define APP_MAIN_H

/* One-time setup: LVGL/display, dashboard UI, backlight PWM, CAN gauges.
 * Call once from inside StartDefaultTask(), before its infinite loop. */
void AppMain_Init(void);

/* Call once per iteration of StartDefaultTask()'s infinite loop, in place
 * of (or alongside) its osDelay(). Drives lv_timer_handler() and pulls
 * updated CAN values into the UI. Non-blocking; caller still owns the loop
 * timing (an osDelay(5) between calls is plenty). */
void AppMain_Run(void);

#endif /* APP_MAIN_H */
