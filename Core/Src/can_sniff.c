/* ============================================================================
 *  can_sniff.c — passive HS-CAN change detector. See can_sniff.h for why.
 * ========================================================================== */
#include "can_sniff.h"
#include "cluster_config.h"
#include "fdcan.h"                /* hfdcan1 */
#include <string.h>

typedef struct {
    uint16_t id;
    uint8_t  len;
    bool     primed;              /* first frame seen: baseline captured      */
    uint8_t  cur[8];
    uint16_t changes[8];
    uint32_t last_ms[8];
    /* Distinct values ever seen per byte, in FIRST-SEEN order (capped). This is
     * what isolates a selector from a counter: a PRNDL byte only ever takes the
     * handful of detent codes and holds each for seconds, so it collects a
     * small, stable set; a counter/checksum saturates SNIFF_DISTINCT_MAX almost
     * at once. And because the set is kept in first-seen order, sweeping the
     * lever P->R->N->D->M->1->2->3 makes the value list read out as the map. */
    uint8_t  nvals[8];
    uint8_t  vals[8][SNIFF_DISTINCT_MAX];
    /* Min/max ever seen per byte. This is the counterpart to the distinct set:
     * an analogue signal (coolant, oil, EGT ramping on warm-up) saturates the
     * distinct set and drops out of the candidate view, but its min..max span
     * grows large -- so ranking by span surfaces exactly the temperatures and
     * pressures the distinct view hides. */
    uint8_t  vmin[8];
    uint8_t  vmax[8];
} sniff_id_t;

static sniff_id_t tbl[SNIFF_MAX_IDS];
static uint8_t    n_ids;
static bool       active;

/* Rolling 1-second frame rate. This is the one number that tells "bus asleep"
 * (0) apart from "receiving but nothing I track is moving" (nonzero) -- the
 * exact ambiguity a frozen frame counter created. Bucketed lazily on read. */
static uint16_t   fps_val, fps_cnt;
static uint32_t   fps_t0;

/* ------------------------------------------------------------------ filter */
/* Acceptance is switched between "OBD replies only" and "everything". The wide
 * filter feeds ordinary bus traffic into the same 16-deep RX FIFO the OBD
 * replies use, and a display flush can stall the drain for a few hundred ms,
 * so frames WILL be lost while sniffing. That is fine for this job: a selector
 * position is broadcast continuously and held for seconds, so a dropped frame
 * costs nothing. It is not fine as a permanent mode, hence the switch. */
/* State of the last switch, not a history: a later successful switch clears
 * it, so "FILTER ERR" on SNIFF means the filter in force now is the wrong one. */
static bool filter_err;

static bool apply_filter(bool promiscuous)
{
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterIndex  = 0;
    f.FilterType   = FDCAN_FILTER_RANGE;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = promiscuous ? 0x000u : OBD_RESP_ECM;
    f.FilterID2    = promiscuous ? 0x7FFu : OBD_RESP_TCM2;
    /* Legal while running (READY|BUSY); refused only in RESET/ERROR state. */
    filter_err = (HAL_FDCAN_ConfigFilter(&hfdcan1, &f) != HAL_OK);
    return !filter_err;
}

void can_sniff_reset(void)
{
    memset(tbl, 0, sizeof tbl);
    n_ids = 0;
    fps_val = 0; fps_cnt = 0; fps_t0 = HAL_GetTick();
}

/* Frames received in the last ~1 s. Bucket rolls over on read; called from the
 * UI refresh (~25 Hz) while the SNIFF page is up, which is often enough. */
uint16_t can_sniff_fps(void)
{
    uint32_t now = HAL_GetTick();
    if (now - fps_t0 >= 1000u) {
        fps_val = fps_cnt;
        fps_cnt = 0;
        fps_t0  = now;
    }
    return fps_val;
}

/* If the wide filter cannot be installed, sniffing stays off: the page would
 * otherwise sit on an empty table while OBD polling is paused for nothing.
 * Leaving is always honoured (OBD polling resumes); if the narrow filter
 * cannot be restored, the extra frames only reach can_sniff_feed, which
 * ignores them while inactive. */
void can_sniff_set_active(bool on)
{
    if (on == active) return;
    if (on) {
        can_sniff_reset();
        if (!apply_filter(true)) return;
        active = true;
    } else {
        active = false;
        (void)apply_filter(false);
    }
}

bool can_sniff_is_active(void)    { return active; }
bool can_sniff_filter_error(void) { return filter_err; }

uint8_t  can_sniff_id_count(void)    { return n_ids; }

/* -------------------------------------------------------------------- feed */
void can_sniff_feed(uint16_t id, const uint8_t *data, uint8_t len, uint32_t rx_ms)
{
    if (!active) return;
    if (len > 8u) len = 8u;
    if (fps_cnt < 0xFFFFu) fps_cnt++;

    sniff_id_t *e = NULL;
    for (uint8_t i = 0; i < n_ids; i++) {
        if (tbl[i].id == id) { e = &tbl[i]; break; }
    }
    if (e == NULL) {
        /* Table full: keep what we have rather than thrashing. A truck's HS-CAN
         * fits well inside SNIFF_MAX_IDS; overflowing means something is wrong
         * (extended IDs leaking in), and silently evicting would hide it. */
        if (n_ids >= SNIFF_MAX_IDS) return;
        e = &tbl[n_ids++];
        e->id = id;
    }
    e->len = len;

    /* Receive time, not now: after a stall the drain delivers a backlog, and
     * "changed 20 ms ago" must not be a frame that arrived 400 ms ago. */
    uint32_t now = rx_ms;
    for (uint8_t b = 0; b < len; b++) {
        /* Record the value in the distinct set (first sight included, so a byte
         * that never changes still shows its one value). Linear scan of <=
         * SNIFF_DISTINCT_MAX; saturates and stops growing for chatty bytes. */
        bool known = false;
        for (uint8_t k = 0; k < e->nvals[b]; k++)
            if (e->vals[b][k] == data[b]) { known = true; break; }
        if (!known && e->nvals[b] < SNIFF_DISTINCT_MAX)
            e->vals[b][e->nvals[b]++] = data[b];

        if (!e->primed) {                    /* first sight is the baseline,  */
            e->cur[b]  = data[b];            /* not a change                  */
            e->vmin[b] = data[b];
            e->vmax[b] = data[b];
            continue;
        }
        if (data[b] < e->vmin[b]) e->vmin[b] = data[b];
        if (data[b] > e->vmax[b]) e->vmax[b] = data[b];
        if (e->cur[b] != data[b]) {
            e->cur[b]     = data[b];
            e->last_ms[b] = now;
            if (e->changes[b] < 0xFFFFu) e->changes[b]++;
        }
    }
    e->primed = true;
}

/* ------------------------------------------------------------- candidates */
/* Bytes with a small distinct-value set: the selector shape. Ranked by most
 * recent change so a byte you just stepped floats up. Counters/checksums have
 * saturated nvals and are excluded; static bytes (nvals < 2) are not candidates.
 * The returned vals[] are in first-seen order, i.e. the order you swept. */
uint8_t can_sniff_candidates(sniff_cand_t *out, uint8_t max)
{
    uint32_t now = HAL_GetTick();
    uint8_t  n   = 0;

    for (uint8_t slot = 0; slot < max; slot++) {
        const sniff_id_t *best_e = NULL;
        uint8_t  best_b  = 0;
        uint32_t best_ms = 0;

        for (uint8_t i = 0; i < n_ids; i++) {
            const sniff_id_t *e = &tbl[i];
            for (uint8_t b = 0; b < e->len; b++) {
                if (e->nvals[b] < 2u) continue;                 /* never moved   */
                if (e->nvals[b] >= SNIFF_DISTINCT_MAX) continue;/* saturated=noise*/
                if (e->changes[b] == 0u) continue;
                uint32_t ms = e->last_ms[b];
                if (ms <= best_ms) continue;
                bool taken = false;
                for (uint8_t k = 0; k < n; k++)
                    if (out[k].id == e->id && out[k].byte_idx == b) { taken = true; break; }
                if (taken) continue;
                best_e = e; best_b = b; best_ms = ms;
            }
        }
        if (best_e == NULL) break;

        out[n].id       = best_e->id;
        out[n].byte_idx = best_b;
        out[n].nvals    = best_e->nvals[best_b];
        for (uint8_t k = 0; k < best_e->nvals[best_b]; k++)
            out[n].vals[k] = best_e->vals[best_b][k];
        out[n].changes  = best_e->changes[best_b];
        out[n].age_ms   = now - best_ms;
        n++;
    }
    return n;
}

/* ----------------------------------------------------------------- movers */
/* Bytes with the widest min..max span: the analogue shape. A coolant or oil
 * temperature climbing on warm-up, a rail pressure or EGT jumping on a throttle
 * blip -- these have a large span and are what the candidate view discards.
 * Fast free-running counters also span widely, so they are excluded by change
 * count (a real sensor moves far fewer times than a per-frame counter). Ranked
 * by span, widest first. */
uint8_t can_sniff_movers(sniff_mover_t *out, uint8_t max)
{
    uint32_t now = HAL_GetTick();
    uint8_t  n   = 0;

    for (uint8_t slot = 0; slot < max; slot++) {
        const sniff_id_t *best_e = NULL;
        uint8_t  best_b   = 0;
        uint16_t best_span = 2;                         /* ignore <=2 (noise)  */

        for (uint8_t i = 0; i < n_ids; i++) {
            const sniff_id_t *e = &tbl[i];
            for (uint8_t b = 0; b < e->len; b++) {
                if (e->changes[b] == 0u) continue;
                if (e->changes[b] > SNIFF_MOVER_MAX_CH) continue;  /* counter   */
                uint16_t span = (uint16_t)e->vmax[b] - e->vmin[b];
                if (span <= best_span) continue;
                bool taken = false;
                for (uint8_t k = 0; k < n; k++)
                    if (out[k].id == e->id && out[k].byte_idx == b) { taken = true; break; }
                if (taken) continue;
                best_e = e; best_b = b; best_span = span;
            }
        }
        if (best_e == NULL) break;

        out[n].id       = best_e->id;
        out[n].byte_idx = best_b;
        out[n].vmin     = best_e->vmin[best_b];
        out[n].vmax     = best_e->vmax[best_b];
        out[n].cur      = best_e->cur[best_b];
        out[n].changes  = best_e->changes[best_b];
        out[n].age_ms   = now - best_e->last_ms[best_b];
        n++;
    }
    return n;
}
