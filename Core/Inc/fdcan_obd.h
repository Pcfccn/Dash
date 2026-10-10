/* ============================================================================
 *  fdcan_obd.h
 *  Read-only OBD-II over FDCAN1 (classic CAN 2.0, 500 kbps) on STM32H743.
 *  NOTE: H7 uses the FDCAN peripheral, NOT bxCAN. OBD = classic frames.
 *
 *  RX: the FDCAN interrupt copies frames into a ring, obd_rx_poll() checks
 *  and decodes them from the main loop (polled fallback if the interrupt
 *  cannot be enabled).
 * ========================================================================== */
#ifndef FDCAN_OBD_H
#define FDCAN_OBD_H

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "cluster_config.h"          /* metric_key_t, M_COUNT */

/* Live values decoded from the bus. UI reads this.
 *
 * Every float starts as NaN meaning "never received". That distinction matters
 * on a real ECM: a PID this calibration does not support simply never answers,
 * and a plain 0.0f would render as a believable reading (a 0 V battery, a 0 °C
 * oil temp). The UI shows "--" for NaN, so the screen only ever claims a value
 * the bus actually produced. */
typedef struct {
    float   speed, rpm, cool, oil, iat, load, boost, rail, egt, battery;
    float   atf;
    float   oil_press;       /* bar, ECM PID 0xA22C (see obd_oilp_probe)       */
    int8_t  gear;            /* TCM current gear: -1 = unknown, 0 = N, 1..8 = D */
    int8_t  sel_range;       /* selector 0x1F5.b3: -1 unknown, 1 P 2 R 3 N 4 D  */
    bool    mil;
    uint8_t dtc_count;       /* emission-related DTC count from PID 0x01      */
    bool    mil_valid;       /* PID 0x01 answered one of its last 3 requests  */
    uint32_t mil_upd_ms;     /* receive time of the last PID 0x01 answer      */
    bool    can_ok;          /* set false if no valid frame within timeout     */

    /* ---- DID discovery aids (DIAG page) --------------------------------
     * The GM-enhanced DIDs are still being identified for this truck, so the
     * raw gear byte and the last negative response are surfaced rather than
     * silently dropped: a stuck raw byte means "wrong DID", while an NRC means
     * "right module, wrong/unsupported identifier". */
    uint8_t gear_raw;        /* last raw byte from the gear DID                */
    uint8_t last_nrc_sid;    /* service that was rejected (0 = none seen)      */
    uint8_t last_nrc;        /* its negative-response code                     */

    /* ---- freshness: HAL tick of the last valid decode, per metric. Stamped
     * on every decode, also when the value did not change. See
     * obd_is_fresh() and metric_stale_ms[] in cluster_config.h. */
    uint32_t upd_ms[M_COUNT];
    uint32_t sel_upd_ms;     /* selector broadcast                             */
} obd_data_t;

extern volatile obd_data_t g_obd;

void obd_init(FDCAN_HandleTypeDef *hfdcan);   /* filters + start               */
void obd_rx_poll(void);                       /* drain RX FIFO0 (call often)    */
void obd_poll_tick(void);                     /* round-robin requests           */
void obd_watchdog_tick_1hz(void);             /* flips can_ok if bus went quiet */
void obd_demo_tick(void);                     /* OBD_DEMO: inject test values    */

/* Is metric k (or the selector) recent enough to show? Works on g_obd or on a
 * snapshot of it. A NaN value is "no data" regardless of this. */
bool obd_is_fresh(const volatile obd_data_t *d, metric_key_t k, uint32_t now);
bool obd_sel_fresh(const volatile obd_data_t *d, uint32_t now);
/* MIL / DTC count usable: PID 0x01 answered one of its last 3 attempts AND
 * within MIL_STALE_MS. The attempt count alone never expires while nothing
 * is asked (SNIFF pauses polling, a stalled scheduler). */
bool obd_mil_fresh(const volatile obd_data_t *d, uint32_t now);

/* Hook implemented by the UI layer: called after a value changes. */
void obd_on_update(void);

/* CAN controller health, for the SNIFF and DIAG pages. Counters saturate. */
typedef struct {
    bool     bus_off;           /* current fault state                          */
    bool     err_passive;
    uint16_t rx_lost;           /* RX FIFO0 overflow events + RX ring drops      */
    uint16_t rx_hwm;            /* most frames ever waiting in the RX ring       */
    bool     rx_irq;            /* true: interrupt RX; false: polled fallback    */
    uint16_t busoff_recover;    /* bus-off recovery attempts                     */
    uint16_t rx_bad;            /* received frames dropped as truncated/malformed */
    uint16_t tx_fail;           /* requests the TX FIFO refused                  */
    uint16_t start_fail;        /* controller restarts refused (retried)         */
    uint16_t txn_capped;        /* transactions ended by the 10 s total cap       */
} obd_health_t;
void obd_can_health(obd_health_t *h);

/* Oil-pressure source, for DIAG: which way PID 0xA22C is being read, the last
 * refusal of each service tried, and the last raw byte. A refusal of $2C ends
 * the search (OILP_NONE): this calibration has no such parameter. */
typedef enum {
    OILP_VIA22 = 0,             /* $22 A22C (tried first)                       */
    OILP_DEFINE,                /* $22 refused: define the packet with $2C next */
    OILP_READ,                  /* packet defined: read it with $AA             */
    OILP_NONE                   /* $2C refused too: not polled any more         */
} obd_oilp_mode_t;
typedef struct {
    uint8_t mode;               /* obd_oilp_mode_t                              */
    uint8_t nrc22, nrc2c, nrcaa;/* last NRC per service, 0 = none seen          */
    bool    have_raw;
    uint8_t raw;                /* last raw byte, A x 4 kPa                     */
} obd_oilp_probe_t;
void obd_oilp_probe(obd_oilp_probe_t *p);

#endif /* FDCAN_OBD_H */
