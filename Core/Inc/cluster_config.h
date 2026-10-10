/* ============================================================================
 *  cluster_config.h
 *  Holden Colorado RG 2.8 Duramax (E98 ECM) — instrument cluster display
 *  Target: STM32 + SN65HVD230, HS-CAN 500 kbps, 320x480, READ-ONLY
 *
 *  Single source of truth for:
 *    1) metric keys, gauge ranges and colour thresholds (from the HTML mockup;
 *       the thresholds are not from an engine calibration — tune them)
 *    2) per-metric freshness limits
 *    3) the CAN IDs and OBD addresses we use
 *
 *  Modules polled: the ECM (0x7E0 -> 0x7E8) and the trans controller
 *  (0x7E2 -> 0x7EA). mode 04 (clear codes) is intentionally NOT used.
 * ========================================================================== */

#ifndef CLUSTER_CONFIG_H
#define CLUSTER_CONFIG_H

#include <stdint.h>

/* ---- Bench demo -----------------------------------------------------------
 * 1 = inject synthetic test values into g_obd (no CAN needed): all colours,
 *     thresholds and the animated coolant are visible on the desk.
 * 0 = real OBD-II over the bus. SET TO 0 BEFORE USING IN THE CAR.            */
#define OBD_DEMO 0

/* ---- Bus / addressing ---------------------------------------------------- */
#define CAN_BITRATE_BPS        500000u
#define OBD_REQ_FUNCTIONAL     0x7DFu   /* broadcast request                   */
#define OBD_REQ_ECM            0x7E0u   /* physical request to engine ECM      */
#define OBD_REQ_TCM            0x7E1u   /* physical request to transmission    */
#define OBD_RESP_ECM           0x7E8u   /* ECM response (req + 8)              */
#define OBD_RESP_TCM           0x7E9u   /* TCM response                        */

/* On the RG Colorado the transmission controller that serves ATF temp
 * (DID 0x1940) and current gear (DID 0x199A) answers on the 7E2/7EA pair,
 * NOT 7E1/7E9 (community-confirmed; verify on the car). The mode-22 poller in
 * fdcan_obd.c requests the TCM here and the RX filter accepts 0x7E8..0x7EA. */
#define OBD_REQ_TCM2           0x7E2u   /* physical request to trans (ATF/gear)*/
#define OBD_RESP_TCM2          0x7EAu   /* its response                        */

/* Broadcast (not request/response) frame carrying the selector range/PRNDL,
 * identified with the SNIFF page: byte 3 = 01 P / 02 R / 03 N / 04 D, and in D
 * byte 2 carries the manual/commanded gear. See docs/sniff-selector.md. */
#define CAN_ID_SELECTOR        0x1F5u

/* Oil-pressure candidate #1 (byte 3), found with SNIFF ANALOG 2026-07-23 and
 * REJECTED on 2026-08-23: it does not track RPM. Raw byte kept on DIAG only. */
#define CAN_ID_OILP_BCAST      0x1BAu

/* Oil-pressure candidate #2 (byte 2, range 0x27..0xFF on SNIFF ANALOG), still
 * UNVERIFIED. Both candidates are captured raw and shown next to live RPM on
 * DIAG; neither feeds the OIL P tile. TEST SCAFFOLD — once the real source is
 * known, drop the loser (its filter in obd_init + RX branch) so the frame stops
 * loading the shared RX FIFO. */
#define CAN_ID_OILP_CAND2      0x0C9u

/* OBD service (mode) bytes */
#define OBD_MODE_CURRENT       0x01u    /* live data (SAE J1979)               */
#define OBD_MODE_FREEZE        0x02u
#define OBD_MODE_DTC_STORED    0x03u
#define OBD_MODE_DTC_PENDING   0x07u
#define OBD_MODE_ENHANCED      0x22u    /* UDS ReadDataByIdentifier (2-byte DID)*/

/* ---- State / severity ---------------------------------------------------- */
typedef enum {
    ST_OK   = 0,
    ST_INFO = 1,   /* informational only, never alarms                        */
    ST_WARN = 2,
    ST_CRIT = 3
} metric_state_t;

/* Which thresholds are meaningful for a metric */
typedef enum {
    THR_NONE      = 0,      /* raw value, no colour logic (speed, rpm-info…)   */
    THR_HIGH_ONLY = 1,      /* temps: warn/crit when value rises               */
    THR_WINDOW    = 2,      /* battery: warn/crit on BOTH low and high sides   */
    THR_INFO      = 3       /* shown, but always ST_INFO                       */
} thr_kind_t;

/* ---- Metric keys (index into metrics[]) ---------------------------------- */
typedef enum {
    M_SPEED = 0, M_RPM, M_COOL, M_OIL, M_ATF, M_EGT, M_BOOST,
    M_BATTERY, M_IAT, M_LOAD, M_RAIL,
    M_OILP,
    M_GEAR,
    M_COUNT
} metric_key_t;

/* ---- Threshold table ----------------------------------------------------- */
typedef struct {
    const char *label;
    thr_kind_t  kind;
    float       scale_min;   /* bar/gauge range low                           */
    float       scale_max;   /* bar/gauge range high                          */
    float       warn_low;    /* used by THR_WINDOW                            */
    float       crit_low;    /* used by THR_WINDOW                            */
    float       warn_high;   /* used by THR_HIGH_ONLY / THR_WINDOW            */
    float       crit_high;
    uint8_t     decimals;
} metric_cfg_t;

/* Values mirror METRICS in the HTML reference. Tune to your sensor location. */
static const metric_cfg_t metrics[M_COUNT] = {
/*  label          kind           min    max   warnLo critLo warnHi critHi dec */
  { "SPEED",       THR_NONE,       0,    240,     0,     0,     0,     0,   0 },
  { "RPM",         THR_HIGH_ONLY,  0,   4500,     0,     0,  3800,  4300,   0 },
  { "COOLANT",     THR_HIGH_ONLY, 40,    120,     0,     0,    93,    97,   0 },
  { "OIL",         THR_HIGH_ONLY, 40,    140,     0,     0,   120,   130,   0 },
  { "ATF",         THR_HIGH_ONLY, 40,    150,     0,     0,   120,   130,   0 },
  { "EGT",         THR_HIGH_ONLY,  0,    800,     0,     0,   650,   720,   0 }, /* exhaust temp   */
  { "BOOST",       THR_INFO,       0,    2.5f,    0,     0,     0,     0,   1 }, /* bar gauge      */
  { "BATTERY",     THR_WINDOW,     9,     16,  12.0f, 11.5f, 15.0f, 15.5f,  1 },
  { "IAT",         THR_INFO,     -20,    100,     0,     0,     0,     0,   0 },
  { "LOAD",        THR_INFO,       0,    100,     0,     0,     0,     0,   0 },
  { "RAIL",        THR_INFO,       0,   2000,     0,     0,     0,     0,   0 }, /* bar            */
  { "OIL P",       THR_WINDOW,     0,      7,   0.8f,  0.4f,   8.0f,  9.0f,  1 }, /* bar; low = danger, high never trips within 0-7 */
  { "GEAR",        THR_NONE,       0,      8,     0,     0,     0,     0,   0 }  /* TCM D1..D6     */
};

/* ---- Freshness ------------------------------------------------------------
 * How long a decoded value stays valid without a new reading, ms (0 = never
 * goes stale). Past its limit a value renders as "--" and stops driving the
 * alert strip, even while broadcasts keep the CAN link "live" — before, a
 * frozen ECM left the last RPM/speed/coolant on screen indefinitely.
 * Generous on purpose until DIAG's LOOP has been measured in the car: one
 * request goes out per loop iteration (an unanswered one holds the slot
 * 150 ms), RPM roughly every 1.5 iterations, MAP and speed every ~3, the
 * medium band (coolant, IAT, rail, battery, load, ATF, gear) every ~18, the
 * oil-temp DID every 16. Tighten once real update intervals are known. */
static const uint16_t metric_stale_ms[M_COUNT] = {
    [M_SPEED]   = 2000,  [M_RPM]  = 2000,  [M_BOOST] = 2000,
    [M_COOL]    = 15000, [M_OIL]  = 15000, [M_ATF]   = 15000, [M_EGT] = 15000,
    [M_BATTERY] = 15000, [M_IAT]  = 15000, [M_LOAD]  = 15000, [M_RAIL] = 15000,
    [M_GEAR]    = 15000,
    [M_OILP]    = 0,     /* no source: stays NaN anyway */
};
#define SEL_STALE_MS 2000u   /* selector broadcast 0x1F5 */

/* Generic state resolver */
static inline metric_state_t metric_state(metric_key_t k, float v) {
    const metric_cfg_t *m = &metrics[k];
    switch (m->kind) {
        case THR_HIGH_ONLY:
            if (v >= m->crit_high) return ST_CRIT;
            if (v >= m->warn_high) return ST_WARN;
            return ST_OK;
        case THR_WINDOW:
            if (v >= m->crit_high || v <= m->crit_low) return ST_CRIT;
            if (v >= m->warn_high || v <= m->warn_low) return ST_WARN;
            return ST_OK;
        case THR_INFO: return ST_INFO;
        default:       return ST_OK;
    }
}

/* ---- Where each metric comes from ------------------------------------------
 * Not a table here any more: the old pid_map[] was documentation nobody read
 * by code and had drifted from the poller. The requests live in
 * obd_poll_tick() and the decoders in fdcan_obd.c; what this truck actually
 * answers (and how it was verified) is the signal table in CLAUDE.md. */

#endif /* CLUSTER_CONFIG_H */
