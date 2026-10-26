/* odo.h — own odometer: TOTAL / TRIP / A, from GPS speed or the car's speed.
 *
 * Distance is the integral of speed over the samples' receive times, never a
 * sum of position fixes (those wander metres at a standstill):
 *   - GPS (RMC Doppler speed) whenever good samples arrive, trapezoid between
 *     consecutive samples no more than ODO_MAX_DT_MS apart; below
 *     ODO_STILL_KMH counts as 0 (drift at a standstill);
 *   - OBD speed (as shown, i.e. already scaled by SPEED_GPS_SCALE) only while
 *     no good GPS sample came for ODO_GPS_HOLD_MS — tunnels, car parks, before
 *     the first fix. Each new stretch starts where the last integrated one
 *     ended (whichever source it came from), so a hand-over neither counts a
 *     stretch twice nor drops the GPS hold window or the gap before the first
 *     sample of the other source. The two sources' receive times are not in
 *     order (the loop feeds the newest GPS fix before it decodes the CAN
 *     frames of the same period), so every time comparison is signed: an OBD
 *     speed a few ms older than the GPS fix already counted means "GPS
 *     present", not "49 days late".
 * TRIP resets by itself when the first GPS time of a power-up is more than
 * ODO_TRIP_GAP_S after the last saved one (parked longer than 30 min); what
 * was driven before that fix stays in the new TRIP. A is reset by hand; TOTAL
 * never. Pure logic: tests/host/test_odo.c. docs/future/gps-odometer-fuel.md. */
#ifndef ODO_H
#define ODO_H

#include <stdbool.h>
#include <stdint.h>

#define ODO_MAX_DT_MS    2000u   /* longer gap between samples: not integrated   */
#define ODO_GPS_HOLD_MS  1500u   /* GPS counts as present this long after a good sample */
#define ODO_STILL_KMH    1.5f    /* below: standing still                        */
#define ODO_TRIP_GAP_S   1800u   /* parked longer than this: new TRIP            */

typedef enum { ODO_SRC_NONE = 0, ODO_SRC_GPS, ODO_SRC_OBD } odo_src_t;

typedef struct {
    double   total_m, trip_m, a_m;   /* metres                                    */
    double   trip_s;                 /* moving time in TRIP, s                    */
    double   boot_m, boot_s;         /* driven / moving since power-up            */
    uint32_t utc_saved;              /* last saved GPS time (s since 2000), 0 = none */
    bool     utc_checked;            /* the auto-reset decision has been made     */
    bool     trip_auto_reset;        /* ... and it reset TRIP                     */
    /* integration state */
    bool     gps_prev, gps_ever;
    float    gps_prev_v;
    uint32_t gps_prev_t, gps_last_t;
    bool     obd_prev;
    float    obd_prev_v;
    uint32_t obd_prev_t;
    odo_src_t src;                   /* source of the last distance added         */
    uint32_t  src_t;                 /* ... and when                              */
    bool      cover;                 /* distance integrated up to cover_t ...     */
    uint32_t  cover_t;               /* ... by either source (hand-over point)    */
} odo_t;

void odo_init(odo_t *o, uint32_t total_m, uint32_t trip_m, uint32_t a_m,
              uint32_t trip_s, uint32_t utc_saved);
/* A GPS RMC sample: speed km/h, good = fix and quality gate passed. */
void odo_gps(odo_t *o, float v_kmh, bool good, uint32_t t_ms);
/* An OBD speed decode (km/h as shown), with the frame's receive time. */
void odo_obd(odo_t *o, float v_kmh, uint32_t t_ms);
/* GPS time, s since 2000-01-01; the first call of a power-up decides TRIP. */
void odo_utc(odo_t *o, uint32_t utc_s);
void odo_reset_trip(odo_t *o);
void odo_reset_a(odo_t *o);
/* Distance was added within ODO_MAX_DT_MS of now: the car is moving. */
bool odo_moving(const odo_t *o, uint32_t now);
odo_src_t odo_source(const odo_t *o, uint32_t now);   /* NONE when nothing for 2 s */

/* Civil date/time (UTC) -> s since 2000-01-01, 0 when out of range. */
uint32_t odo_utc_from_civil(uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hh, uint8_t mm, uint8_t ss);

#endif /* ODO_H */
