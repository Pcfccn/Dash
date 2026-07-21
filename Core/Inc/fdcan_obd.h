/* ============================================================================
 *  fdcan_obd.h
 *  Read-only OBD-II over FDCAN1 (classic CAN 2.0, 500 kbps) on STM32H743.
 *  NOTE: H7 uses the FDCAN peripheral, NOT bxCAN. OBD = classic frames.
 *
 *  Adapted for this project: RX is POLLED (obd_rx_poll) from the main loop
 *  instead of using the FDCAN interrupt, since the project has no FDCAN NVIC
 *  handler wired.
 * ========================================================================== */
#ifndef FDCAN_OBD_H
#define FDCAN_OBD_H

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Live values decoded from the bus. UI reads this.
 *
 * Every float starts as NaN meaning "never received". That distinction matters
 * on a real ECM: a PID this calibration does not support simply never answers,
 * and a plain 0.0f would render as a believable reading (a 0 V battery, a 0 °C
 * oil temp). The UI shows "--" for NaN, so the screen only ever claims a value
 * the bus actually produced. */
typedef struct {
    float   speed, rpm, cool, oil, iat, load, boost, rail, egt, battery;
    float   atf, soot, dpf_dp, egr_t, since_regen;
    int8_t  gear;            /* TCM current gear: -1 = unknown, 0 = N, 1..8 = D */
    int8_t  sel_range;       /* selector 0x1F5.b3: -1 unknown, 1 P 2 R 3 N 4 D  */
    bool    mil;
    uint8_t dtc_count;
    bool    can_ok;          /* set false if no valid frame within timeout     */

    /* ---- DID discovery aids (DIAG page) --------------------------------
     * The GM-enhanced DIDs are still being identified for this truck, so the
     * raw gear byte and the last negative response are surfaced rather than
     * silently dropped: a stuck raw byte means "wrong DID", while an NRC means
     * "right module, wrong/unsupported identifier". */
    uint8_t gear_raw;        /* last raw byte from the gear DID                */
    uint8_t last_nrc_sid;    /* service that was rejected (0 = none seen)      */
    uint8_t last_nrc;        /* its negative-response code                     */
} obd_data_t;

extern volatile obd_data_t g_obd;

void obd_init(FDCAN_HandleTypeDef *hfdcan);   /* filters + start               */
void obd_rx_poll(void);                       /* drain RX FIFO0 (call often)    */
void obd_poll_tick(void);                     /* round-robin requests           */
void obd_watchdog_tick_1hz(void);             /* flips can_ok if bus went quiet */
void obd_demo_tick(void);                     /* OBD_DEMO: inject test values    */

/* Hook implemented by the UI layer: called after a value changes. */
void obd_on_update(void);

/* CAN controller health, for the SNIFF page. Any pointer may be NULL.
 * bus_off / err_passive are the current fault state; lost is a running count of
 * RX FIFO0 overflow events; recover counts bus-off recovery attempts. */
void obd_can_health(bool *bus_off, bool *err_passive,
                    uint16_t *lost, uint16_t *recover);

#endif /* FDCAN_OBD_H */
