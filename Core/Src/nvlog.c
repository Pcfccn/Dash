/* nvlog.c — journal of settings and odometer in the SPI NOR, see nvlog.h. */
#include "nvlog.h"
#include "spi_nor.h"
#include <string.h>

/* ---- record codec ---------------------------------------------------------
 *  0 magic | 1 version | 2 bl_day | 3 bl_night | 4..7 seq | 8..11 total
 * 12..15 trip | 16..19 A | 20..23 trip_s | 24..27 utc | 28..31 crc32(0..27)
 * Little-endian. A slot of all 0xFF is erased (magic 0xFF never matches). */
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t nv_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

void nv_encode(uint8_t *r, const nv_data_t *d, uint32_t seq)
{
    r[0] = NV_MAGIC; r[1] = NV_VERSION; r[2] = d->bl_day; r[3] = d->bl_night;
    put32(r + 4, seq);
    put32(r + 8, d->odo_total_m);
    put32(r + 12, d->odo_trip_m);
    put32(r + 16, d->odo_a_m);
    put32(r + 20, d->trip_s);
    put32(r + 24, d->utc);
    put32(r + 28, nv_crc32(r, 28));
}

bool nv_decode(const uint8_t *r, nv_data_t *d, uint32_t *seq)
{
    if (r[0] != NV_MAGIC || r[1] != NV_VERSION) return false;
    if (get32(r + 28) != nv_crc32(r, 28)) return false;
    d->bl_day = r[2]; d->bl_night = r[3];
    *seq           = get32(r + 4);
    d->odo_total_m = get32(r + 8);
    d->odo_trip_m  = get32(r + 12);
    d->odo_a_m     = get32(r + 16);
    d->trip_s      = get32(r + 20);
    d->utc         = get32(r + 24);
    return true;
}

static bool erased(const uint8_t *r)
{
    for (unsigned i = 0; i < NV_SLOT; i++) if (r[i] != 0xFFu) return false;
    return true;
}

/* ---- state ---------------------------------------------------------------- */
typedef enum { W_IDLE = 0, W_ERASING, W_PROGRAMMING } wstate_t;

static struct {                       /* file scope: the host test resets it */
    nv_state_t state;
    nv_data_t  data;                  /* newest known (loaded or queued)      */
    uint32_t   seq;                   /* newest record on the flash           */
    uint16_t   sector, slot;          /* next slot to try                     */
    uint16_t   errors;
    bool       pending;               /* data not yet on the flash            */
    uint32_t   gen;                   /* bumped by every nvlog_save()         */
    bool       erase_pending;
    uint16_t   erase_sector;
    uint16_t   erasing;               /* sector of the erase in progress      */
    wstate_t   w;
    uint32_t   w_seq;                 /* seq of the record being programmed   */
    uint32_t   w_gen;                 /* gen of the data being programmed     */
    uint8_t    w_rec[NV_SLOT];
} nv;

static uint32_t sector_addr(uint16_t s) { return NV_BASE + (uint32_t)s * 4096u; }
static uint32_t slot_addr(uint16_t s, uint16_t k) { return sector_addr(s) + (uint32_t)k * NV_SLOT; }
static uint16_t next_sector(uint16_t s) { return (uint16_t)((s + 1u) % NV_SECTORS); }

/* Newer in the wrap-safe sense (a record a second takes 136 years to wrap). */
static bool seq_newer(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

/* First valid record of a sector, past any torn slots at its start. An
 * erased slot ends the search: the writer only moves forward, so nothing
 * valid follows it. Usually one read (slot 0 valid, or erased). Looking at
 * slots 0 and 1 only lost a sector whose first two writes were torn: the
 * previous sector then looked newest, and this one, holding the newer
 * records, was erased as "the next sector" (review C8). */
static bool sector_head(uint16_t s, uint32_t *seq)
{
    uint8_t r[NV_SLOT];
    nv_data_t d;
    for (uint16_t k = 0; k < NV_SLOTS; k++) {
        if (!nor_read(slot_addr(s, k), r, NV_SLOT)) return false;
        if (nv_decode(r, &d, seq)) return true;
        if (erased(r)) return false;
    }
    return false;
}

void nvlog_init(void)
{
    memset(&nv, 0, sizeof nv);
    if (!nor_init()) { nv.state = NV_NO_FLASH; return; }
    nv.state = NV_EMPTY;

    /* The sector holding the newest record is the one whose first record is
     * newest: records go in ring order, one sector after another. */
    bool found = false;
    uint16_t best = 0;
    uint32_t best_seq = 0;
    for (uint16_t s = 0; s < NV_SECTORS; s++) {
        uint32_t q;
        if (sector_head(s, &q) && (!found || seq_newer(q, best_seq))) {
            found = true; best = s; best_seq = q;
        }
    }

    if (found) {
        /* Inside it: the newest valid slot, then the first erased one after it. */
        uint8_t r[NV_SLOT];
        int last = -1;
        for (uint16_t k = 0; k < NV_SLOTS; k++) {
            nv_data_t d;
            uint32_t  q;
            if (!nor_read(slot_addr(best, k), r, NV_SLOT)) break;
            if (nv_decode(r, &d, &q) && (last < 0 || seq_newer(q, nv.seq))) {
                nv.data = d; nv.seq = q; last = k;
            }
        }
        if (last < 0) {
            /* The head was there a moment ago, now nothing reads: an
             * unreliable chip. RAM only — writing zeros as "the newest
             * record" would replace TOTAL with 0. */
            memset(&nv.data, 0, sizeof nv.data);
            nv.state = NV_NO_FLASH;
            return;
        }
        nv.state  = NV_LOADED;
        nv.sector = best;
        nv.slot   = (uint16_t)(last + 1);
    } else {
        nv.sector = 0;
        nv.slot   = 0;
    }
    /* The next sector must be clean before the writer gets there. It may hold
     * records from the previous trip round the ring (older, so not needed) or
     * a half-finished erase; erase it now rather than check it. */
    nv.erase_pending = true;
    nv.erase_sector  = next_sector(nv.sector);
}

const nv_data_t *nvlog_data(void) { return &nv.data; }

void nvlog_save(const nv_data_t *d)
{
    nv.data    = *d;
    nv.pending = true;
    nv.gen++;
}

void nvlog_info(nv_info_t *o)
{
    o->state   = nv.state;
    o->jedec   = nor_jedec();
    o->seq     = nv.seq;
    o->sector  = nv.sector;
    o->slot    = nv.slot;
    o->errors  = nv.errors;
    o->pending = nv.pending;
}

static void error(void) { if (nv.errors < 0xFFFFu) nv.errors++; }

/* Move to the next sector; its successor gets erased ahead of time. */
static void enter_next_sector(void)
{
    nv.sector = next_sector(nv.sector);
    nv.slot   = 0;
    nv.erase_pending = true;
    nv.erase_sector  = next_sector(nv.sector);
}

void nvlog_service(void)
{
    if (nv.state == NV_NO_FLASH) return;
    if (nor_busy()) return;

    if (nv.w == W_ERASING) {
        nv.w = W_IDLE;                /* done; a failed erase shows up as slots
                                       * that are not erased, handled below    */
        /* The sector being written had to be cleaned first: the one after it
         * still needs its erase ahead of time. */
        if (nv.erasing == nv.sector && !nv.erase_pending) {
            nv.erase_pending = true;
            nv.erase_sector  = next_sector(nv.sector);
        }
    } else if (nv.w == W_PROGRAMMING) {
        uint8_t back[NV_SLOT];
        nv.w = W_IDLE;
        if (nor_read(slot_addr(nv.sector, nv.slot), back, NV_SLOT) &&
            memcmp(back, nv.w_rec, NV_SLOT) == 0) {
            nv.seq = nv.w_seq;
            nv.state = NV_LOADED;
            /* Done, unless newer data was queued while the program ran. */
            if (nv.w_gen == nv.gen) nv.pending = false;
        } else {
            error();                  /* stays pending: retried in the next slot */
            /* The record may be on the flash all the same (a read glitch):
             * its seq is used up, so the retry — possibly newer data — gets
             * a higher one and wins at boot (review C8). */
            nv.seq = nv.w_seq;
        }
        nv.slot++;
    }

    if (nv.erase_pending) {
        if (nor_erase4k(sector_addr(nv.erase_sector))) {
            nv.erase_pending = false;
            nv.erasing = nv.erase_sector;
            nv.w = W_ERASING;
        } else {
            error();
        }
        return;
    }

    if (!nv.pending) return;

    /* Find an erased slot: skip torn or never-erased ones. A sector whose
     * slots run out is left behind; when the very first slot of a sector we
     * just entered is not erased, that sector is erased first. */
    for (unsigned tries = 0; tries < NV_SLOTS + 1u; tries++) {
        if (nv.slot >= NV_SLOTS) enter_next_sector();
        uint8_t r[NV_SLOT];
        if (!nor_read(slot_addr(nv.sector, nv.slot), r, NV_SLOT)) { error(); return; }
        if (erased(r)) break;
        if (nv.slot == 0u) {          /* entered a dirty sector: clean it      */
            nv.erase_pending = true;
            nv.erase_sector  = nv.sector;
            return;
        }
        nv.slot++;
    }

    nv.w_seq = nv.seq + 1u;
    nv.w_gen = nv.gen;
    nv_encode(nv.w_rec, &nv.data, nv.w_seq);
    if (nor_program(slot_addr(nv.sector, nv.slot), nv.w_rec, NV_SLOT)) {
        nv.w = W_PROGRAMMING;
    } else {
        error();
        nv.slot++;
    }
}
