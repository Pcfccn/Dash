#ifndef CAN_GAUGES_H
#define CAN_GAUGES_H

#include <stdint.h>
#include <stdbool.h>

/* 1 = fake sweeping gauges (no hardware). 0 = real OBD-II polling over FDCAN1. */
#define CAN_GAUGES_SIMULATE 0

/* NO_DATA marks a value we haven't received yet (PID unsupported or not
 * answered), so the UI can show "--" instead of a bogus number. */
#define GAUGE_NO_DATA (-1000)

typedef struct {
    int32_t  speed_kmh;    /* PID 0x0D */
    int32_t  rpm;          /* PID 0x0C */
    int32_t  coolant_c;    /* PID 0x05, or GAUGE_NO_DATA */
    int32_t  oil_c;        /* PID 0x5C, or GAUGE_NO_DATA */
    int32_t  batt_mv;      /* PID 0x42 control-module voltage in mV, or GAUGE_NO_DATA */
    int32_t  oil_kpa;      /* engine oil pressure, kPa. NOT in standard OBD-II -> stays
                            * GAUGE_NO_DATA until a GM-specific (Mode 22) PID is added. */
    int8_t   gear;         /* not in std OBD-II: -1 = unknown */
    bool     mil_on;       /* PID 0x01: check-engine lamp */
    bool     mil_valid;    /* true once PID 0x01 has actually been answered */
    uint8_t  dtc_count;    /* PID 0x01: number of stored DTCs */
    uint16_t dtc[6];       /* raw DTC codes from Mode 03 */
    uint8_t  dtc_n;        /* how many entries in dtc[] are valid */
} GaugeValues_t;

/* Diagnostics for bring-up: total CAN frames received and the last CAN id
 * seen. Shown on the small status screen so you can tell, in the car, whether
 * the bus is being received at all. */
extern volatile uint32_t g_can_rx_frames;
extern volatile uint32_t g_can_last_id;

/* Call once after MX_FDCAN1_Init() (real mode) / after RTOS start (sim mode). */
void CanGauges_Init(void);

/* Call from the FDCAN RX-new-message interrupt (real mode only). */
void CanGauges_RxCallback(uint32_t id, const uint8_t *data, uint8_t len);

/* Call periodically (e.g. every 20-50 ms) from the CAN/gauge task.
 * Returns true if values changed since the last call. */
bool CanGauges_Poll(GaugeValues_t *out);

#endif /* CAN_GAUGES_H */
