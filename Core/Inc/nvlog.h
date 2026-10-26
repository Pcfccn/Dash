/* nvlog.h — settings and odometer kept across power-off, in the SPI NOR.
 *
 * Journal in the last 1 MB of the W25Q64 (256 sectors of 4 KB, 128 slots of
 * 32 bytes each): every save appends one record [magic ver data seq crc32].
 * At boot the newest record with a good CRC wins, so a save cut short by the
 * ignition going off leaves a torn slot that is simply skipped — the previous
 * record stands. The sector after the one being written is erased ahead of
 * time, so moving on never waits for an erase.
 *
 * Wear: one record a second while driving, 2 h a day = 7200 records = 56
 * erases a day, spread over 256 sectors = ~80 erases a sector a year against
 * the 100 000 the W25Q64 is rated for (docs/future/gps-odometer-fuel.md §4).
 *
 * All flash work is non-blocking: nvlog_save() only queues (the newest data
 * wins), nvlog_service() from the loop advances the erase / program / verify
 * steps while the chip is not busy. Pure logic on spi_nor.h, so the host test
 * (tests/host/test_nvlog.c) runs it against a simulated NOR, power cuts
 * included. */
#ifndef NVLOG_H
#define NVLOG_H

#include <stdbool.h>
#include <stdint.h>

#define NV_BASE        (7u * 1024u * 1024u)   /* last 1 MB of the 8 MB chip */
#define NV_SECTORS     256u
#define NV_SLOT        32u
#define NV_SLOTS       (4096u / NV_SLOT)      /* per sector: 128 */
#define NV_MAGIC       0x5Au
#define NV_VERSION     1u

/* What is kept. Zero / defaults when nothing was ever saved. */
typedef struct {
    uint8_t  bl_day, bl_night;       /* brightness profiles, %; 0 = not set   */
    uint32_t odo_total_m;            /* odometer: TOTAL, TRIP, A, in metres    */
    uint32_t odo_trip_m;
    uint32_t odo_a_m;
    uint32_t trip_s;                 /* moving time of TRIP, s                 */
    uint32_t utc;                    /* s since 2000-01-01 at the last save with
                                      * GPS time; 0 = never known              */
} nv_data_t;

typedef enum {
    NV_NO_FLASH = 0,                 /* chip did not answer: RAM only          */
    NV_EMPTY,                        /* chip fine, no record yet               */
    NV_LOADED,                       /* a record was found and loaded          */
} nv_state_t;

typedef struct {
    nv_state_t state;
    uint32_t   jedec;
    uint32_t   seq;                  /* of the newest record written / loaded
                                      * (a failed verify uses its seq up)      */
    uint16_t   sector, slot;         /* where the next record goes             */
    uint16_t   errors;               /* failed programs / verifies / erases    */
    bool       pending;              /* a save is queued                       */
} nv_info_t;

void             nvlog_init(void);   /* nor_init + scan; call once at boot     */
const nv_data_t *nvlog_data(void);   /* loaded at boot, then the last queued   */
void             nvlog_save(const nv_data_t *d);
void             nvlog_service(void);/* from the loop: advance the flash work  */
void             nvlog_info(nv_info_t *out);

/* Record codec, exposed for the tests. */
void     nv_encode(uint8_t *rec, const nv_data_t *d, uint32_t seq);
bool     nv_decode(const uint8_t *rec, nv_data_t *d, uint32_t *seq);
uint32_t nv_crc32(const uint8_t *p, uint32_t n);

#endif /* NVLOG_H */
