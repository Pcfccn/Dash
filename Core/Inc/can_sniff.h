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

#define SNIFF_MAX_IDS   80u      /* distinct standard IDs tracked             */
#define SNIFF_TOP_N      8u      /* rows the UI asks for                      */

typedef struct {
    uint16_t id;
    uint8_t  byte_idx;
    uint8_t  prev;               /* value before the most recent change       */
    uint8_t  cur;
    uint16_t changes;            /* total changes seen since reset            */
    uint32_t age_ms;             /* how long ago it last changed              */
} sniff_hit_t;

/* Enable/disable listening. Enabling widens the FDCAN acceptance filter to the
 * whole standard-ID space and clears the table; disabling restores the narrow
 * OBD-only filter. Not free: the wide filter lets ordinary bus traffic compete
 * for the same RX FIFO the OBD replies use, so this must not stay on. */
void can_sniff_set_active(bool on);
bool can_sniff_is_active(void);

/* Feed one received frame (called for non-OBD IDs from the RX drain). */
void can_sniff_feed(uint16_t id, const uint8_t *data, uint8_t len);

/* Fill out[] with the most interesting recent changes, best first.
 * Returns how many rows were written (<= max). */
uint8_t can_sniff_top(sniff_hit_t *out, uint8_t max);

/* Distinct IDs seen and total frames counted, for a "is it even listening?" readout. */
uint8_t  can_sniff_id_count(void);
uint32_t can_sniff_frame_count(void);

/* Forget all history (the UI offers this so the user can mark a clean point
 * just before moving the selector). */
void can_sniff_reset(void);

#endif /* CAN_SNIFF_H */
