/* Host-side tests for Core/Src/nvlog.c against a simulated W25Q64: NOR
 * semantics (erase -> 0xFF, program only clears bits, busy after program and
 * erase), power cuts in the middle of a program or an erase, ring wrap. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../Core/Src/nvlog.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---- simulated NOR -------------------------------------------------------- */
static uint8_t  mem[NOR_SIZE];
static bool     chip_ok = true;
static int      busy_left;            /* nor_busy() calls until idle            */
static int      tear_bytes = -1;      /* >= 0: the next program writes only this many */
static bool     tear_erase;           /* the next erase leaves garbage           */
static uint32_t stuck_addr = 0xFFFFFFFFu;   /* a byte that never programs        */
static unsigned n_program, n_erase;
static uint32_t glitch_addr = 0xFFFFFFFFu;  /* the next read there returns a flipped bit */
static int      n_reads, read_fail_from = -1;   /* reads from this one on fail     */

bool nor_init(void) { return chip_ok; }
uint32_t nor_jedec(void) { return chip_ok ? 0xEF4017u : 0u; }

bool nor_read(uint32_t a, void *buf, uint32_t len)
{
    if (!chip_ok || a + len > NOR_SIZE) return false;
    if (read_fail_from >= 0 && n_reads++ >= read_fail_from) return false;
    memcpy(buf, mem + a, len);
    if (glitch_addr >= a && glitch_addr < a + len) {
        ((uint8_t *)buf)[glitch_addr - a] ^= 0x01u;
        glitch_addr = 0xFFFFFFFFu;
    }
    return true;
}

bool nor_busy(void)
{
    if (busy_left > 0) { busy_left--; return true; }
    return false;
}

bool nor_program(uint32_t a, const void *buf, uint32_t len)
{
    if (!chip_ok || busy_left > 0 || len > NOR_PAGE || (a / NOR_PAGE) != ((a + len - 1u) / NOR_PAGE)) return false;
    const uint8_t *p = buf;
    uint32_t n = (tear_bytes >= 0) ? (uint32_t)tear_bytes : len;
    tear_bytes = -1;
    for (uint32_t i = 0; i < n && i < len; i++) {
        if (a + i == stuck_addr) continue;
        mem[a + i] &= p[i];
    }
    n_program++;
    busy_left = 2;
    return true;
}

bool nor_erase4k(uint32_t a)
{
    if (!chip_ok || busy_left > 0 || (a % NOR_SECTOR) != 0u) return false;
    memset(mem + a, 0xFF, NOR_SECTOR);
    if (tear_erase) { tear_erase = false; for (int i = 0; i < 64; i++) mem[a + 37u * (uint32_t)i] = 0x12; }
    n_erase++;
    busy_left = 5;
    return true;
}

static void fresh_chip(uint8_t fill)
{
    memset(mem, fill, sizeof mem);
    chip_ok = true; busy_left = 0; tear_bytes = -1; tear_erase = false;
    stuck_addr = 0xFFFFFFFFu; n_program = n_erase = 0;
    glitch_addr = 0xFFFFFFFFu; n_reads = 0; read_fail_from = -1;
}

/* Service until nothing is pending and the chip is idle (bounded). */
static void settle(void)
{
    for (int i = 0; i < 2000; i++) {
        nvlog_service();
        if (!nv.pending && nv.w == W_IDLE && !nv.erase_pending && busy_left == 0) return;
    }
}

static nv_data_t data_n(uint32_t n)
{
    nv_data_t d = { 0 };
    d.bl_day = (uint8_t)(10u + n % 90u); d.bl_night = 30;
    d.odo_total_m = 1000u + n; d.odo_trip_m = n; d.odo_a_m = 2u * n; d.trip_s = n / 3u; d.utc = 700000000u + n;
    return d;
}

static bool same(const nv_data_t *a, const nv_data_t *b)
{
    return a->bl_day == b->bl_day && a->bl_night == b->bl_night && a->odo_total_m == b->odo_total_m &&
           a->odo_trip_m == b->odo_trip_m && a->odo_a_m == b->odo_a_m && a->trip_s == b->trip_s &&
           a->utc == b->utc;
}

/* ---- tests ---------------------------------------------------------------- */
static void t_codec(void)
{
    uint8_t r[NV_SLOT];
    nv_data_t d = data_n(7), e;
    uint32_t q;
    nv_encode(r, &d, 1234u);
    CHECK(nv_decode(r, &e, &q) && q == 1234u && same(&d, &e));
    r[10] ^= 1u;
    CHECK(!nv_decode(r, &e, &q));                       /* CRC catches a flipped bit */
    memset(r, 0xFF, sizeof r);
    CHECK(!nv_decode(r, &e, &q));                       /* erased slot is no record  */
}

static void t_empty_then_saved(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.state == NV_EMPTY && nvlog_data()->odo_total_m == 0u);
    nv_data_t d = data_n(5);
    nvlog_save(&d);
    settle();
    nvlog_info(&inf);
    CHECK(inf.state == NV_LOADED && inf.seq == 1u && !inf.pending && inf.errors == 0u);
    nvlog_init();                                       /* power-up again */
    nvlog_info(&inf);
    CHECK(inf.state == NV_LOADED && inf.seq == 1u && same(nvlog_data(), &d));
}

static void t_no_flash(void)
{
    fresh_chip(0xFF);
    chip_ok = false;
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.state == NV_NO_FLASH && inf.jedec == 0u);
    nv_data_t d = data_n(1);
    nvlog_save(&d);
    for (int i = 0; i < 10; i++) nvlog_service();
    CHECK(n_program == 0u && same(nvlog_data(), &d));  /* RAM copy still usable */
}

static void t_many_saves_across_sectors(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t d = { 0 };
    for (uint32_t n = 1; n <= 1000u; n++) { d = data_n(n); nvlog_save(&d); settle(); }
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.seq == 1000u && inf.errors == 0u);
    CHECK(inf.sector == 1000u / NV_SLOTS);              /* 7 full sectors, then the 8th */
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 1000u && same(nvlog_data(), &d));
    /* next save goes on, no gap and no rewrite */
    d = data_n(1001); nvlog_save(&d); settle();
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 1001u && same(nvlog_data(), &d));
}

static void t_ring_wraps(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t d = { 0 };
    uint32_t total = NV_SECTORS * NV_SLOTS + 300u;      /* once round and a bit */
    for (uint32_t n = 1; n <= total; n++) { d = data_n(n); nvlog_save(&d); settle(); }
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.seq == total && same(nvlog_data(), &d) && inf.errors == 0u);
}

static void t_power_cut_mid_program(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t a = data_n(10), b = data_n(11);
    nvlog_save(&a); settle();
    tear_bytes = 13;                                    /* ignition off mid-write */
    nvlog_save(&b);
    nvlog_service();                                    /* program starts, power goes */
    nvlog_init();                                       /* next power-up */
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.seq == 1u && same(nvlog_data(), &a));     /* previous record stands */
    /* the torn slot is skipped, the next save lands after it */
    nvlog_save(&b); settle();
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 2u && same(nvlog_data(), &b));
}

static void t_power_cut_first_slot_of_sector(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t d = { 0 };
    for (uint32_t n = 1; n <= NV_SLOTS; n++) { d = data_n(n); nvlog_save(&d); settle(); }  /* sector 0 full */
    tear_bytes = 5;
    nv_data_t e = data_n(500);
    nvlog_save(&e);
    nvlog_service();                                    /* torn slot 0 of sector 1 */
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.seq == NV_SLOTS && same(nvlog_data(), &d));
    nvlog_save(&e); settle();                           /* goes to sector 1, slot 1 */
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == NV_SLOTS + 1u && same(nvlog_data(), &e));   /* found via slot 1 */
}

static void t_dirty_chip_and_torn_erase(void)
{
    fresh_chip(0x00);                                   /* never erased: all zeros */
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.state == NV_EMPTY);
    nv_data_t d = { 0 };
    for (uint32_t n = 1; n <= 300u; n++) { d = data_n(n); nvlog_save(&d); settle(); }
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 300u && same(nvlog_data(), &d));
    /* an erase that leaves garbage: the writer skips the bad slots */
    tear_erase = true;
    for (uint32_t n = 301; n <= 600u; n++) { d = data_n(n); nvlog_save(&d); settle(); }
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 600u && same(nvlog_data(), &d));
}

static void t_verify_failure_retries(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t a = data_n(1);
    nvlog_save(&a); settle();
    stuck_addr = NV_BASE + 1u * NV_SLOT + 6u;           /* slot 1, a byte inside */
    nv_data_t b = data_n(2);
    nvlog_save(&b); settle();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.errors == 1u && inf.seq == 3u && !inf.pending);   /* seq 2 used up */
    nvlog_init();
    nvlog_info(&inf);
    CHECK(inf.seq == 3u && same(nvlog_data(), &b));
}

/* Review C8: the program succeeded but its verify read glitched, and newer
 * data was queued meanwhile. The retry used to get the same seq, and at boot
 * the first of two equal seqs — the older data — won. */
static void t_c8_verify_glitch_newer_wins(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t a = data_n(1), b = data_n(2), c = data_n(3);
    nvlog_save(&a); settle();
    nvlog_save(&b);
    for (int i = 0; i < 20 && nv.w != W_PROGRAMMING; i++) nvlog_service();
    CHECK(nv.w == W_PROGRAMMING);
    glitch_addr = slot_addr(nv.sector, nv.slot) + 9u;   /* b is fine on the chip */
    nvlog_save(&c);                                     /* queued while programming */
    settle();
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(same(nvlog_data(), &c) && inf.seq == 3u);
}

/* Review C8: the first two writes of a sector failed their verify (bad
 * cells, a supply dip) and the third landed; then the power went. That
 * sector holds the newest record; it used to be missed (only slots 0 and 1
 * were looked at) and then erased as "the next sector". */
static void t_c8_two_torn_slots_at_sector_start(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t d = { 0 };
    for (uint32_t n = 1; n <= NV_SLOTS; n++) { d = data_n(n); nvlog_save(&d); settle(); }
    nv_data_t e = data_n(1000);
    tear_bytes = 7;                                     /* slot 0 of sector 1 */
    nvlog_save(&e);
    for (int i = 0; i < 40 && n_program == NV_SLOTS; i++) nvlog_service();
    tear_bytes = 7;                                     /* slot 1 as well */
    settle();                                           /* lands in slot 2 */
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.errors == 2u && inf.sector == 1u && inf.slot == 3u);
    nvlog_init();                                       /* power-up */
    nvlog_info(&inf);
    CHECK(same(nvlog_data(), &e) && inf.sector == 1u);
    for (int i = 0; i < 200; i++) nvlog_service();      /* erase-ahead runs */
    nvlog_init();
    CHECK(same(nvlog_data(), &e));                      /* still there */
}

/* Review C8: the head was found, then the chip stopped reading. Loaded zeros
 * would have been saved over TOTAL; the store goes RAM-only instead. */
static void t_c8_read_failure_mid_scan(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t d = data_n(7);
    nvlog_save(&d); settle();
    read_fail_from = NV_SECTORS;                        /* head scan: one read a sector */
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(inf.state == NV_NO_FLASH);
    nv_data_t z = { 0 };
    nvlog_save(&z);
    for (int i = 0; i < 20; i++) nvlog_service();
    read_fail_from = -1;
    nvlog_init();
    CHECK(same(nvlog_data(), &d));                      /* nothing was overwritten */
}

static void t_newest_queued_wins(void)
{
    fresh_chip(0xFF);
    nvlog_init();
    nv_data_t a = data_n(1), b = data_n(2);
    nvlog_save(&a);
    nvlog_service();                                    /* erase of the next sector starts */
    nvlog_save(&b);                                     /* replaces a before it is written */
    settle();
    nvlog_init();
    nv_info_t inf; nvlog_info(&inf);
    CHECK(same(nvlog_data(), &b));
    /* saved while a program is running: written right after it */
    nv_data_t c = data_n(3);
    nvlog_save(&a);
    for (int i = 0; i < 6 && nv.w != W_PROGRAMMING; i++) nvlog_service();
    nvlog_save(&c);
    settle();
    nvlog_init();
    CHECK(same(nvlog_data(), &c));
}

int main(void)
{
    struct { const char *name; void (*fn)(void); } T[] = {
        { "codec",                  t_codec },
        { "empty_then_saved",       t_empty_then_saved },
        { "no_flash",               t_no_flash },
        { "many_saves_sectors",     t_many_saves_across_sectors },
        { "ring_wraps",             t_ring_wraps },
        { "power_cut_mid_program",  t_power_cut_mid_program },
        { "power_cut_first_slot",   t_power_cut_first_slot_of_sector },
        { "dirty_chip_torn_erase",  t_dirty_chip_and_torn_erase },
        { "verify_failure_retries", t_verify_failure_retries },
        { "c8_verify_glitch",       t_c8_verify_glitch_newer_wins },
        { "c8_two_torn_slots",      t_c8_two_torn_slots_at_sector_start },
        { "c8_read_fail_mid_scan",  t_c8_read_failure_mid_scan },
        { "newest_queued_wins",     t_newest_queued_wins },
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        int f0 = fails;
        T[i].fn();
        printf("%-26s %s\n", T[i].name, fails == f0 ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
