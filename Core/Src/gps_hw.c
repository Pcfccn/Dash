/* USART2 for the GPS receiver, register level — see gps_hw.h.
 *
 *   PA2 = USART2_TX (AF7) -> module RX
 *   PA3 = USART2_RX (AF7) <- module TX, pulled up so an unplugged module
 *                            reads as an idle line, not noise
 *
 * Kernel clock: D2 PCLK1 (selected explicitly below; 112.5 MHz in Dash.ioc),
 * oversampling 16, 8N1, no FIFO. RX by interrupt into a 1 KB ring, so no DMA
 * and nothing to clean or invalidate in the D-cache. What 1 KB covers depends
 * on the traffic, not the baud rate alone (review C7 PERF-03): a line that
 * never pauses fills it in 1024 / 11520 B/s = 89 ms at 115200, but the module
 * sends bursts — configured (RMC + GGA at 10 Hz, ~140 B per fix) ~1.4 KB/s,
 * about 0.7 s; its default set at 9600, at most 960 B/s, over 1 s. Against a
 * ~0.2-0.4 s display flush stall that is enough, not a guarantee: a fuller
 * ring drops bytes and counts them (gps_hw_overruns(), GPS overruns).
 * Each line end ('\r' or '\n') is stamped with its receive time in the IRQ,
 * so a sentence that waited out a stall is not taken as new (C7 SW-06).
 * TX (a few configuration commands, rarely) is polled with a time bound. */

#include "gps_hw.h"
#include <stdbool.h>
#include "main.h"

#define GPS_RXQ_LEN 1024u                    /* power of two */
static volatile uint8_t  rxq[GPS_RXQ_LEN];
static volatile uint32_t rxq_head;           /* written by the IRQ  */
static volatile uint32_t rxq_tail;           /* read by the loop    */
static volatile uint16_t overruns;

/* Receive time of every '\r' / '\n' in rxq, in the same order: one stamp per
 * line end, pushed and popped with its byte. 128 = 64 lines of CR LF, more
 * than 1 KB of NMEA (shortest real sentence ~30 B) can hold. */
#define GPS_EOLQ_LEN 128u                    /* power of two */
static volatile uint32_t eolq[GPS_EOLQ_LEN];
static volatile uint32_t eolq_head;          /* written by the IRQ  */
static volatile uint32_t eolq_tail;          /* read by the loop    */
static uint32_t line_ms;                     /* stamp of the last line end read */

#define GPS_TX_TIMEOUT_MS 100u               /* a 64-char command at 9600 is ~67 ms */

void gps_hw_set_baud(uint32_t baud)
{
    USART2->CR1 = 0;                         /* UE off: BRR and format writable */
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->PRESC = 0;
    USART2->BRR = (HAL_RCC_GetPCLK1Freq() + baud / 2u) / baud;
    USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    rxq_tail = rxq_head;                     /* drop bytes received at the old rate */
    eolq_tail = eolq_head;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE | USART_CR1_UE;
}

void gps_hw_init(uint32_t baud)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART234578_CONFIG(RCC_USART234578CLKSOURCE_D2PCLK1);
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef gi = { 0 };
    gi.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gi.Mode      = GPIO_MODE_AF_PP;
    gi.Pull      = GPIO_PULLUP;
    gi.Speed     = GPIO_SPEED_FREQ_LOW;
    gi.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gi);

    gps_hw_set_baud(baud);
    /* Priority 6, like the display DMA: below configMAX_SYSCALL_INTERRUPT_PRIORITY;
     * the handler makes no RTOS calls anyway. */
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)
{
    uint32_t isr = USART2->ISR;
    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        if ((isr & USART_ISR_ORE) && overruns < 0xFFFFu) overruns++;
        USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }
    while (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t  b    = (uint8_t)USART2->RDR;
        uint32_t next = (rxq_head + 1u) & (GPS_RXQ_LEN - 1u);
        bool     eol  = (b == '\r' || b == '\n');
        uint32_t enext = (eolq_head + 1u) & (GPS_EOLQ_LEN - 1u);
        if (next == rxq_tail || (eol && enext == eolq_tail)) {
            if (overruns < 0xFFFFu) overruns++;   /* ring full: the loop stalled */
            continue;
        }
        rxq[rxq_head] = b;
        if (eol) eolq[eolq_head] = HAL_GetTick();
        __DMB();                             /* data visible before the indices */
        if (eol) eolq_head = enext;
        rxq_head = next;
    }
}

int gps_hw_getc(void)
{
    uint32_t t = rxq_tail;
    if (t == rxq_head) return -1;
    uint8_t b = rxq[t];
    if (b == '\r' || b == '\n') {          /* its stamp is next in eolq */
        uint32_t e = eolq_tail;
        line_ms = eolq[e];
        eolq_tail = (e + 1u) & (GPS_EOLQ_LEN - 1u);
    }
    __DMB();
    rxq_tail = (t + 1u) & (GPS_RXQ_LEN - 1u);
    return b;
}

uint32_t gps_hw_line_ms(void) { return line_ms; }

void gps_hw_write(const char *s)
{
    uint32_t t0 = HAL_GetTick();
    while (*s) {
        while (!(USART2->ISR & USART_ISR_TXE_TXFNF))
            if (HAL_GetTick() - t0 > GPS_TX_TIMEOUT_MS) return;
        USART2->TDR = (uint8_t)*s++;
    }
    /* the caller may change the baud rate next: let the last bit out */
    while (!(USART2->ISR & USART_ISR_TC))
        if (HAL_GetTick() - t0 > GPS_TX_TIMEOUT_MS) return;
}

uint16_t gps_hw_overruns(void) { return overruns; }
uint32_t gps_hw_now_ms(void)   { return HAL_GetTick(); }
