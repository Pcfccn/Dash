/* ============================================================================
 *  can_sniff.c — passive HS-CAN change detector. See can_sniff.h for why.
 * ========================================================================== */
#include "can_sniff.h"
#include "cluster_config.h"
#include "fdcan.h"                /* hfdcan1 */
#include <string.h>

/* A byte that changes this many times is a counter, a checksum or a live
 * sensor -- never a selector position. Ranked out so it cannot bury the one
 * byte that moved twice when the lever moved twice. */
#define SNIFF_CHATTY  40u

typedef struct {
    uint16_t id;
    uint8_t  len;
    bool     primed;              /* first frame seen: baseline captured      */
    uint8_t  cur[8];
    uint8_t  prev[8];
    uint16_t changes[8];
    uint32_t last_ms[8];
} sniff_id_t;

static sniff_id_t tbl[SNIFF_MAX_IDS];
static uint8_t    n_ids;
static uint32_t   n_frames;
static bool       active;

/* ------------------------------------------------------------------ filter */
/* Acceptance is switched between "OBD replies only" and "everything". The wide
 * filter feeds ordinary bus traffic into the same 16-deep RX FIFO the OBD
 * replies use, and a display flush can stall the drain for a few hundred ms,
 * so frames WILL be lost while sniffing. That is fine for this job: a selector
 * position is broadcast continuously and held for seconds, so a dropped frame
 * costs nothing. It is not fine as a permanent mode, hence the switch. */
static void apply_filter(bool promiscuous)
{
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterIndex  = 0;
    f.FilterType   = FDCAN_FILTER_RANGE;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1    = promiscuous ? 0x000u : OBD_RESP_ECM;
    f.FilterID2    = promiscuous ? 0x7FFu : OBD_RESP_TCM2;
    HAL_FDCAN_ConfigFilter(&hfdcan1, &f);   /* legal while running (READY|BUSY) */
}

void can_sniff_reset(void)
{
    memset(tbl, 0, sizeof tbl);
    n_ids = 0;
    n_frames = 0;
}

void can_sniff_set_active(bool on)
{
    if (on == active) return;
    active = on;
    if (on) can_sniff_reset();
    apply_filter(on);
}

bool can_sniff_is_active(void) { return active; }

uint8_t  can_sniff_id_count(void)    { return n_ids; }
uint32_t can_sniff_frame_count(void) { return n_frames; }

/* -------------------------------------------------------------------- feed */
void can_sniff_feed(uint16_t id, const uint8_t *data, uint8_t len)
{
    if (!active) return;
    if (len > 8u) len = 8u;
    n_frames++;

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

    uint32_t now = HAL_GetTick();
    for (uint8_t b = 0; b < len; b++) {
        if (!e->primed) {                    /* first sight is the baseline,  */
            e->cur[b] = data[b];             /* not a change                  */
            continue;
        }
        if (e->cur[b] != data[b]) {
            e->prev[b]    = e->cur[b];
            e->cur[b]     = data[b];
            e->last_ms[b] = now;
            if (e->changes[b] < 0xFFFFu) e->changes[b]++;
        }
    }
    e->primed = true;
}

/* --------------------------------------------------------------------- top */
/* Most recently changed first, with chatty bytes excluded. Selection sort over
 * the whole table: at most 80*8 candidates, run a couple of times a second. */
uint8_t can_sniff_top(sniff_hit_t *out, uint8_t max)
{
    if (max > SNIFF_TOP_N) max = SNIFF_TOP_N;
    uint32_t now = HAL_GetTick();
    uint32_t taken_ms[SNIFF_TOP_N];
    uint8_t  n = 0;

    for (uint8_t slot = 0; slot < max; slot++) {
        const sniff_id_t *best_e = NULL;
        uint8_t  best_b  = 0;
        uint32_t best_ms = 0;

        for (uint8_t i = 0; i < n_ids; i++) {
            const sniff_id_t *e = &tbl[i];
            for (uint8_t b = 0; b < e->len; b++) {
                if (e->changes[b] == 0u) continue;
                if (e->changes[b] > SNIFF_CHATTY) continue;
                uint32_t ms = e->last_ms[b];
                if (ms <= best_ms) continue;
                bool already = false;              /* skip slots already taken */
                for (uint8_t k = 0; k < n; k++) {
                    if (taken_ms[k] == ms && out[k].id == e->id && out[k].byte_idx == b) {
                        already = true; break;
                    }
                }
                if (already) continue;
                best_e = e; best_b = b; best_ms = ms;
            }
        }
        if (best_e == NULL) break;

        out[n].id       = best_e->id;
        out[n].byte_idx = best_b;
        out[n].prev     = best_e->prev[best_b];
        out[n].cur      = best_e->cur[best_b];
        out[n].changes  = best_e->changes[best_b];
        out[n].age_ms   = now - best_ms;
        taken_ms[n]     = best_ms;
        n++;
    }
    return n;
}
