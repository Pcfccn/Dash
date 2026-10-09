/* fault.h — watchdog, fatal-fault recording and last-reset reporting.
 *
 * A hung or crashed firmware must not leave the last frame on the panel: the
 * ILI9488 keeps its GRAM, so a frozen display shows plausible numbers forever.
 * The IWDG resets the board instead, and the reason is shown on the next boot. */
#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>

/* ---- Bench self-test -------------------------------------------------------
 * 0 = off. With the key held for FAULT_TEST_HOLD_MS:
 *   1 = the superloop hangs  -> expect a reset after ~3 s, DIAG "LAST RESET: WATCHDOG"
 *   2 = MPU-blocked read     -> expect an immediate reset,  DIAG "LAST RESET: HARDFAULT"
 * SET TO 0 BEFORE USING IN THE CAR.                                          */
#define FAULT_TEST         0
#define FAULT_TEST_HOLD_MS 10000u

typedef enum {
    FAULT_NONE = 0,
    FAULT_HARDFAULT,
    FAULT_STACK_OVERFLOW,
    FAULT_ERROR_HANDLER,
} fault_code_t;

void        fault_init(void);              /* first thing at boot: latch + clear reset cause */
void        fault_record(fault_code_t c);  /* call right before a fatal stop; survives reset  */
const char *fault_reset_text(void);        /* why the previous run ended, e.g. "WATCHDOG"     */
bool        fault_reset_abnormal(void);    /* watchdog or recorded fault, not a power-up      */

void        fault_wdg_start_boot(void);    /* start IWDG1 with the long boot timeout (10 s)   */
void        fault_wdg_run_mode(void);      /* switch to the run timeout (3 s) after init      */
void        fault_wdg_kick(void);          /* once per completed superloop iteration          */

#if FAULT_TEST
void        fault_selftest(void);          /* trigger the FAULT_TEST fault                    */
#endif

#endif /* FAULT_H */
