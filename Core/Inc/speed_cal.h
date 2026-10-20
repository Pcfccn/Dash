#ifndef SPEED_CAL_H
#define SPEED_CAL_H

/* Speedometer check against GPS (review C7 CAN-04). SPEED_GPS_SCALE in
 * cluster_config.h is a one-point fit (GPS 60 = 67 on screen); this measures
 * the remaining error at every steady speed, so one drive at 40 / 60 / 80 /
 * 100 km/h shows whether the error is proportional and what the scale should
 * be. It only measures and suggests: SPEED_GPS_SCALE is changed by hand.
 *
 * Each OBD speed (already scaled, as shown) is paired with the GPS sample
 * nearest in time, within SC_PAIR_MS. A pair counts only when:
 *  - the OBD speed has stayed within SC_STEADY_KMH over the last
 *    SC_STEADY_MS (no accelerating, braking or gear changes);
 *  - the speed is at least SC_MIN_KMH (1 km/h steps are 2.5 % at 40);
 *  - the GPS has a fix with >= SC_MIN_SATS satellites and HDOP <= SC_MAX_HDOP
 *    (or no HDOP reported).
 * Sums are kept per band (40-60, 60-80, 80-100, 100+), and the band's factor
 * is sum(GPS) / sum(OBD) — the shown speed is right when it is 1.000. Pure
 * logic, host-tested (tests/host/test_speed_cal.c). */

#include <stdbool.h>
#include <stdint.h>

#define SC_PAIR_MS     100u
#define SC_STEADY_MS   2000u
#define SC_STEADY_KMH  2.0f      /* OBD speed steps ~0.9 km/h: 2-3 values in a cruise */
#define SC_MIN_KMH     40.0f
#define SC_MIN_SATS    6u
#define SC_MAX_HDOP    2.0f
#define SC_BANDS       4u        /* 40-60, 60-80, 80-100, 100+ */

typedef struct {
    uint32_t n;                  /* pairs                       */
    double   gps, obd;           /* sums, km/h                  */
} sc_band_t;

void             speed_cal_reset(void);
/* A GPS RMC: speed, fix valid, satellites, HDOP (NaN = unknown), its receive time. */
void             speed_cal_gps(float kmh, bool fix, uint8_t sats, float hdop, uint32_t t_ms);
/* An OBD speed as shown (after SPEED_GPS_SCALE) and its receive time. */
void             speed_cal_obd(float kmh, uint32_t t_ms);
const sc_band_t *speed_cal_band(uint8_t b);
/* sum(GPS) / sum(OBD) over one band, or over all bands (b = SC_BANDS);
 * NaN without pairs. */
float            speed_cal_factor(uint8_t b);

#endif /* SPEED_CAL_H */
