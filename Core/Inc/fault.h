/* fault.h — watchdog, fatal-fault recording and last-reset reporting.
 *
 * A hung or crashed firmware must not leave the last frame on the panel: the
 * ILI9488 keeps its GRAM, so a frozen display shows plausible numbers forever.
 * The IWDG resets the board instead, and the reason is shown on the next boot. */
#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>

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

void        fault_wdg_start(void);         /* start IWDG1 (cannot be stopped afterwards)      */
void        fault_wdg_kick(void);          /* once per completed superloop iteration          */

#endif /* FAULT_H */
