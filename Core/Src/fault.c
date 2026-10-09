/* fault.c — see fault.h. */
#include "fault.h"
#include "main.h"            /* CMSIS device header: RCC, IWDG1, DBGMCU */
#include "FreeRTOS.h"
#include "task.h"

/* The fault record lives at the top of DTCM: the linker script places every
 * section in RAM_D1, so DTCM is unused, and the startup code never clears it,
 * so the record survives a software or watchdog reset. After a power-up its
 * content is garbage (and not yet ECC-initialised), so it is only read when
 * the reset was not a power-on/brown-out, and rewritten on every boot. */
#define FAULT_MAGIC  0xFA017C0Du
typedef struct {
    uint32_t magic;
    uint32_t code;
} fault_rec_t;
#define FAULT_REC    ((volatile fault_rec_t *)0x2001FFF0u)

static const char *s_reset_text = "?";
static bool        s_abnormal;

void fault_init(void)
{
    uint32_t rsr  = RCC->RSR;
    bool     cold = (rsr & (RCC_RSR_PORRSTF | RCC_RSR_BORRSTF)) != 0u;
    uint32_t code = FAULT_NONE;

    if (!cold && FAULT_REC->magic == FAULT_MAGIC) code = FAULT_REC->code;
    FAULT_REC->magic = FAULT_MAGIC;
    FAULT_REC->code  = FAULT_NONE;

    s_abnormal = true;
    if      (code == FAULT_HARDFAULT)      s_reset_text = "HARDFAULT";
    else if (code == FAULT_STACK_OVERFLOW) s_reset_text = "STACK OVERFLOW";
    else if (code == FAULT_ERROR_HANDLER)  s_reset_text = "HAL ERROR";
    else if (rsr & RCC_RSR_IWDG1RSTF)      s_reset_text = "WATCHDOG";
    else {
        s_abnormal = false;
        if      (rsr & RCC_RSR_PORRSTF)    s_reset_text = "POWER ON";
        else if (rsr & RCC_RSR_BORRSTF)    s_reset_text = "BROWN-OUT";
        else if (rsr & RCC_RSR_SFTRSTF)    s_reset_text = "SOFTWARE";
        else if (rsr & RCC_RSR_PINRSTF)    s_reset_text = "RESET PIN";
        else                               s_reset_text = "OTHER";
    }
    /* Clear the flags so the next boot reports only its own reset. */
    RCC->RSR |= RCC_RSR_RMVF;
}

void fault_record(fault_code_t c)
{
    FAULT_REC->magic = FAULT_MAGIC;
    FAULT_REC->code  = (uint32_t)c;
    __DSB();
}

const char *fault_reset_text(void)     { return s_reset_text; }
bool        fault_reset_abnormal(void) { return s_abnormal; }

/* IWDG1 runs from the 32 kHz LSI, which starting the IWDG switches on.
 * Prescaler /64 -> 500 Hz, reload 1500 -> 3 s. Deliberately generous: one
 * superloop iteration can still hold a blocking full-screen SPI flush
 * (~0.4 s on the wire alone). Tighten once the display path is non-blocking.
 * Frozen while a debugger halts the core, so breakpoints do not reset it. */
#define WDG_PR_DIV64   4u
#define WDG_RELOAD     1500u

void fault_wdg_start(void)
{
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
    IWDG1->KR  = 0xCCCCu;            /* start                         */
    IWDG1->KR  = 0x5555u;            /* unlock PR / RLR               */
    IWDG1->PR  = WDG_PR_DIV64;
    IWDG1->RLR = WDG_RELOAD;
    while (IWDG1->SR != 0u) { }      /* wait until both values latch  */
    IWDG1->KR  = 0xAAAAu;            /* reload with the new value     */
}

void fault_wdg_kick(void)
{
    IWDG1->KR = 0xAAAAu;
}

/* configCHECK_FOR_STACK_OVERFLOW = 2 (FreeRTOSConfig.h). Overrides the weak
 * stub in cmsis_os2.c. Record and stop; the IWDG then resets the board. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    fault_record(FAULT_STACK_OVERFLOW);
    __disable_irq();
    for (;;) { }
}
