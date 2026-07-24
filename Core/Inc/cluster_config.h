/* ============================================================================
 *  cluster_config.h
 *  Holden Colorado RG 2.8 Duramax (E98 ECM) — instrument cluster display
 *  Target: STM32 + SN65HVD230, HS-CAN 500 kbps, 320x480, READ-ONLY
 *
 *  Single source of truth for:
 *    1) colour thresholds  (mirror of METRICS in the HTML reference)
 *    2) CAN / OBD PID map   (how each value is polled)
 *
 *  Only the engine (ECM 0x7E0) and transmission (TCM 0x7E1) modules are
 *  present on the bus we read. No BCM/IPC -> no ambient/outside temp.
 *  Enhanced (mode 22) DIDs marked 0x0000 MUST be filled from an E98/TCM
 *  definition (EFILive / HP Tuners) for the specific RG calibration.
 *  mode 04 (clear codes) is intentionally NOT used.
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

/* Provisional oil-pressure broadcast (byte 3 → /100 = bar), identified with the
 * SNIFF ANALOG page 2026-07-23; still needs a throttle-blip confirmation. Used
 * by both the RX decode and the acceptance filter in fdcan_obd.c. */
#define CAN_ID_OILP_BCAST      0x1BAu

/* Second oil-pressure candidate under test (2026-07-24): 0x0C9 byte 2 showed a
 * wide range (0x27..0xFF) on the SNIFF ANALOG page, plausibly A/36 → bar. Both
 * 0x1BA[3] and 0x0C9[2] are captured raw and shown alongside live RPM on DIAG
 * so a throttle-blip photo reveals which one actually tracks engine speed.
 * TEST SCAFFOLD — once the real source is known, drop the loser (its filter in
 * obd_init + RX branch) to stop the frame loading the shared RX FIFO. */
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

/* ---- PID / source map ---------------------------------------------------- */
typedef enum { SRC_ECM = 0, SRC_TCM = 1 } ecu_src_t;

typedef struct {
    metric_key_t key;
    ecu_src_t    src;
    uint8_t      mode;    /* 0x01 standard, 0x22 enhanced                     */
    uint16_t     pid;     /* mode 01: low byte is the PID; mode 22: full DID  */
    const char  *decode;  /* SAE J1979 decode for the standard ones           */
} pid_map_t;

/*
 * STANDARD entries are SAE J1979 and reliable as-is.
 * ENHANCED entries (mode 0x22, pid=0x0000) are PLACEHOLDERS:
 *   read the real DID from an E98/TCM definition before use.
 * BOOST is derived: boost_bar = (MAP_kPa - BARO_kPa) / 100.
 */
static const pid_map_t pid_map[] = {
  /* key            src      mode              pid      decode (A,B = data bytes) */
  { M_SPEED,        SRC_ECM, OBD_MODE_CURRENT, 0x000D, "A  (km/h)" },
  { M_RPM,          SRC_ECM, OBD_MODE_CURRENT, 0x000C, "((A*256)+B)/4" },
  { M_COOL,         SRC_ECM, OBD_MODE_CURRENT, 0x0005, "A-40  (degC)" },
  { M_OIL,          SRC_ECM, OBD_MODE_CURRENT, 0x005C, "A-40  (modelled)" },
  { M_IAT,          SRC_ECM, OBD_MODE_CURRENT, 0x000F, "A-40  (degC)" },
  { M_LOAD,         SRC_ECM, OBD_MODE_CURRENT, 0x0004, "A*100/255  (%)" },
  { M_BOOST,        SRC_ECM, OBD_MODE_CURRENT, 0x000B, "MAP=A kPa; boost=MAP-baro(0x33)" },
  { M_RAIL,         SRC_ECM, OBD_MODE_CURRENT, 0x0023, "((A*256)+B)/10  (bar)" },
  { M_EGT,          SRC_ECM, OBD_MODE_CURRENT, 0x0078, "sensor bank1; ((A*256+B)/10)-40, pick DPF-zone sensor" },
  { M_BATTERY,      SRC_ECM, OBD_MODE_CURRENT, 0x0042, "((A*256)+B)/1000  (V)" },

  /* ENHANCED / non-standard. DIDs from GM/Torque/EFILive community configs for
   * the 2.8 LWN + E98 — NOT yet verified on this car. Confidence: [H]/[M]/[L].
   * This table is documentation; the actual requests are hardcoded in
   * fdcan_obd.c's obd_poll_tick()/decoders. ATF and GEAR are polled; oil
   * pressure arrives as a passive CAN broadcast (see CAN_ID_OILP_BCAST), not a
   * request — the 0x115C DID returns NRC 22/31 on this E98. The DPF/soot/EGR
   * signals were dropped: this ECM never answered any of their DIDs/PIDs. */
  { M_ATF,          SRC_TCM, OBD_MODE_ENHANCED, 0x1940, "[H] TCM@7E2 A-40 degC — trans fluid temp (Torque 221940) — POLLED" },
  { M_GEAR,         SRC_TCM, OBD_MODE_ENHANCED, 0x199A, "[M] TCM@7E2 current gear = A — verify scaling on car — POLLED" },
};

/*
 * Extra signals not in metrics[] (status, not gauges):
 *   MIL + DTC count : mode 0x01 PID 0x01   (byte A bit7 = MIL, A&0x7F = count)
 *   Stored DTCs     : mode 0x03            (ISO-TP multi-frame; send FC 30 00 00)
 *   Pending DTCs    : mode 0x07
 *   Gear / TCC lock : TCM 0x7E1 enhanced   (DID TBD)
 *   Regen state     : ECM 0x7E0 enhanced   (DID TBD; IDLE/ACTIVE)
 *
 * Polling hint: group the standard mode-01 PIDs into multi-PID requests
 * (GM accepts up to 6 PIDs per frame) to cut latency; poll enhanced DIDs
 * individually; poll DTCs at ~1 Hz. Responses > 7 bytes are ISO-TP.
 */

#endif /* CLUSTER_CONFIG_H */
