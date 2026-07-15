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

/* Live values decoded from the bus. UI reads this. */
typedef struct {
    float   speed, rpm, cool, oil, iat, load, boost, rail, egt, battery;
    float   atf, soot, dpf_dp, egr_t, since_regen;
    bool    mil;
    uint8_t dtc_count;
    bool    can_ok;          /* set false if no valid frame within timeout     */
} obd_data_t;

extern volatile obd_data_t g_obd;

void obd_init(FDCAN_HandleTypeDef *hfdcan);   /* filters + start               */
void obd_rx_poll(void);                       /* drain RX FIFO0 (call often)    */
void obd_poll_tick(void);                     /* round-robin requests           */
void obd_watchdog_tick_1hz(void);             /* flips can_ok if bus went quiet */

/* Hook implemented by the UI layer: called after a value changes. */
void obd_on_update(void);

#endif /* FDCAN_OBD_H */
