/* ============================================================================
 *  can_sniff.h — passive HS-CAN change detector, for finding unknown signals.
 *
 *  The remaining unknowns on this truck (selector range / PRNDL, soot load,
 *  DPF differential pressure, distance since regen) are not answered by any
 *  OBD identifier we have found, but the OEM cluster displayed them -- so they
 *  are broadcast as ordinary frames on the 500 kbps HS-CAN. Asking is the wrong
 *  tool; listening is the right one.
 *
 *  Method: watch every byte of every standard ID and remember when it last
 *  changed and how often. Parked with the ignition on, almost nothing on the
 *  bus moves, so operating one control (the selector) makes exactly its byte
 *  jump to the top of the "changed most recently" list. Bytes that change
 *  constantly (counters, checksums, wheel speeds) are ranked down by their
 *  change count so they cannot bury the interesting one.
 * ========================================================================== */
#ifndef CAN_SNIFF_H
#define CAN_SNIFF_H

#include <stdint.h>
#include <stdbool.h>

#define SNIFF_MAX_IDS      80u   /* distinct standard IDs tracked             */
#define SNIFF_DISTINCT_MAX 10u   /* distinct values kept per byte before it   */
                                 /* is written off as a counter/checksum      */
#define SNIFF_MOVER_MAX_CH 300u  /* above this many changes a wide-span byte  */
                                 /* is a free-running counter, not a sensor   */

/* One low-cardinality byte: a selector-shaped signal and its value set. */
typedef struct {
    uint16_t id;
    uint8_t  byte_idx;
    uint8_t  nvals;                        /* distinct values seen (2..MAX-1)  */
    uint8_t  vals[SNIFF_DISTINCT_MAX];     /* in first-seen (== swept) order   */
    uint16_t changes;
    uint32_t age_ms;
} sniff_cand_t;

/* One wide-span byte: an analogue-shaped signal (temperature, pressure). */
typedef struct {
    uint16_t id;
    uint8_t  byte_idx;
    uint8_t  vmin, vmax, cur;
    uint16_t changes;
    uint32_t age_ms;
} sniff_mover_t;

/* Enable/disable listening. Enabling widens the FDCAN acceptance filter to the
 * whole standard-ID space and clears the table; disabling restores the narrow
 * OBD-only filter. Not free: the wide filter lets ordinary bus traffic compete
 * for the same RX FIFO the OBD replies use, so this must not stay on. */
void can_sniff_set_active(bool on);
bool can_sniff_is_active(void);
bool can_sniff_filter_error(void);   /* an acceptance-filter switch was refused */

/* Feed one received frame (called for non-OBD IDs from the RX drain). */
void can_sniff_feed(uint16_t id, const uint8_t *data, uint8_t len);

/* Bytes whose distinct-value set is small enough to be a selector/state signal
 * (counters excluded). Best = most recently changed. This is the primary
 * discovery view: it names the position byte and lists its codes directly. */
uint8_t can_sniff_candidates(sniff_cand_t *out, uint8_t max);

/* Bytes with the widest min..max span: analogue signals (temperatures,
 * pressures) that the candidate view discards. Widest span first. */
uint8_t can_sniff_movers(sniff_mover_t *out, uint8_t max);

/* Distinct IDs seen, for an "is it even listening?" readout. */
uint8_t  can_sniff_id_count(void);

/* Frames received in the last ~1 s: 0 = bus silent, nonzero = live traffic. */
uint16_t can_sniff_fps(void);

/* Forget all history (the UI offers this so the user can mark a clean point
 * just before moving the selector). */
void can_sniff_reset(void);

#endif /* CAN_SNIFF_H */
