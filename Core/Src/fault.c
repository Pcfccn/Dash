/* fault.c — see fault.h. */
#include "fault.h"
#include "main.h"            /* HAL + CMSIS device header: RCC, IWDG1, DBGMCU */
#include "FreeRTOS.h"
#include "task.h"

/* The fault record lives in .noinit (DTCM, see STM32H743VITX_FLASH.ld): never
 * initialised or cleared by the startup code, so it survives a software or
 * watchdog reset. After a power-up its content is garbage (and its ECC not yet
 * initialised), so it is only read when the reset was not a power-on or
 * brown-out, must pass the magic + inverted-code check, and is rewritten on
 * every boot. */
#define FAULT_MAGIC  0xFA017C0Du
typedef struct {
    uint32_t magic;
    uint32_t code;
    uint32_t code_inv;       /* ~code: rejects a half-written or random record */
} fault_rec_t;
static volatile fault_rec_t s_rec __attribute__((section(".noinit")));

static const char *s_reset_text = "?";
static bool        s_abnormal;

void fault_init(void)
{
    uint32_t rsr  = RCC->RSR;
    bool     cold = (rsr & (RCC_RSR_PORRSTF | RCC_RSR_BORRSTF)) != 0u;
    uint32_t code = FAULT_NONE;

    if (!cold && s_rec.magic == FAULT_MAGIC && s_rec.code_inv == ~s_rec.code)
        code = s_rec.code;
    s_rec.magic    = FAULT_MAGIC;
    s_rec.code     = FAULT_NONE;
    s_rec.code_inv = ~(uint32_t)FAULT_NONE;

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
    s_rec.magic    = FAULT_MAGIC;
    s_rec.code     = (uint32_t)c;
    s_rec.code_inv = ~(uint32_t)c;
    __DSB();
}

const char *fault_reset_text(void)     { return s_reset_text; }
bool        fault_reset_abnormal(void) { return s_abnormal; }

/* IWDG1 runs from the ~32 kHz LSI, which starting the IWDG switches on; the
 * LSI is not trimmed, so real timeouts can differ from nominal by tens of
 * percent. Boot: /256 -> 125 Hz, reload 1250 -> ~10 s, covering clock and
 * peripheral init, RTOS start and the slow display/LVGL bring-up. Run: /64 ->
 * 500 Hz, reload 1500 -> ~3 s, deliberately generous: a full repaint still
 * holds the loop for ~0.2 s of SPI time even with the DMA flush (it was
 * ~0.4 s blocking before); tighten once LOOP on DIAG has been measured on
 * the bench and in the car. Frozen while a
 * debugger halts the core, so breakpoints do not reset it. */
#define WDG_BOOT_PR    6u       /* /256 */
#define WDG_BOOT_RLR   1250u
#define WDG_RUN_PR     4u       /* /64  */
#define WDG_RUN_RLR    1500u

/* PR/RLR updates take a few LSI cycles to reach the counter domain. Bounded:
 * if the LSI never runs the update never completes, and hanging here would be
 * worse than carrying on with whatever timeout is in effect. */
static void wdg_set(uint32_t pr, uint32_t rlr)
{
    IWDG1->KR  = 0x5555u;            /* unlock PR / RLR               */
    IWDG1->PR  = pr;
    IWDG1->RLR = rlr;
    uint32_t t0 = HAL_GetTick();
    for (uint32_t n = 0; IWDG1->SR != 0u && n < 5000000u && (HAL_GetTick() - t0) < 100u; n++) { }
    IWDG1->KR  = 0xAAAAu;            /* reload with the new value     */
}

void fault_wdg_start_boot(void)
{
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
    IWDG1->KR = 0xCCCCu;             /* start (cannot be stopped)     */
    wdg_set(WDG_BOOT_PR, WDG_BOOT_RLR);
}

void fault_wdg_run_mode(void)
{
    wdg_set(WDG_RUN_PR, WDG_RUN_RLR);
}

void fault_wdg_kick(void)
{
    IWDG1->KR = 0xAAAAu;
}

#if FAULT_TEST
void fault_selftest(void)
{
#if FAULT_TEST == 1
    for (;;) { }                     /* the loop stops feeding the IWDG */
#elif FAULT_TEST == 2
    /* MPU region 0 (main.c MPU_Config) forbids 0x60000000..0xDFFFFFFF; the
     * MemManage fault is not enabled, so it escalates to HardFault. */
    (void)*(volatile uint32_t *)0x60000000u;
#endif
}
#endif

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
