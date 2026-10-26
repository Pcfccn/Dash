/* trip_app.h — glue for the TRIP and PERF pages: owns the odometer (odo.c),
 * the acceleration timer (perf.c) and the persistent store (nvlog.c), feeds
 * them GPS fixes and OBD speeds, and decides when to save.
 *
 * Saves: once a second while the car moves, at once when it stops, 2 s after
 * the last brightness change, at once after a counter reset. Brightness
 * profiles live in the same record (app_main.c reads them at boot). */
#ifndef TRIP_APP_H
#define TRIP_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "odo.h"
#include "perf.h"
#include "nvlog.h"
#include "gps.h"

#define TRIP_SAVE_MS      1000u      /* while moving                         */
#define TRIP_BL_SAVE_MS   2000u      /* after the last brightness change     */
#define TRIP_RESET_KMH    3.0f       /* resets only below this (fresh speed) */

typedef enum { TRIP_SEL_TRIP = 0, TRIP_SEL_A, TRIP_SEL_TOTAL, TRIP_SEL_N } trip_sel_t;

void trip_app_init(void);            /* nvlog + odometer from the last record */
/* Brightness profiles from the store; false when none was ever saved. */
bool trip_app_brightness(uint8_t *day, uint8_t *night);
void trip_app_set_brightness(uint8_t day, uint8_t night, uint32_t now);

void trip_app_gps(const gps_state_t *g);          /* each new RMC, fix or not */
void trip_app_obd_speed(float v_kmh, uint32_t rx_ms);
void trip_app_service(uint32_t now);              /* from the loop */

/* ● on TRIP: tap = next counter, hold = reset it (TRIP or A; TOTAL never).
 * ● held on PERF: clear the results. Both only when standing still. */
void trip_app_select_next(void);
bool trip_app_reset_selected(uint32_t now);
bool trip_app_perf_clear(uint32_t now);

const odo_t  *trip_app_odo(void);
const perf_t *trip_app_perf(void);
trip_sel_t    trip_app_selected(void);
bool          trip_app_standing(uint32_t now);   /* fresh speed below TRIP_RESET_KMH */

#endif /* TRIP_APP_H */
