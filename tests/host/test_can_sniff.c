/* Host-side tests for the real Core/Src/can_sniff.c: filter switching and
 * its recovery, the table, candidate/mover ranking and receive timestamps.
 * The HAL is the shared mock (tests/host/mock); hfdcan1 and the HAL calls
 * can_sniff.c makes are defined here. */
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/can_sniff.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ------------------------------------------------------------- mock HAL */
FDCAN_HandleTypeDef hfdcan1;
static uint32_t          now_ms;
static HAL_StatusTypeDef filter_status;
static bool              hw_wide;          /* what the controller has installed */
static int               filter_writes;

uint32_t HAL_GetTick(void) { return now_ms; }

HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *h, FDCAN_FilterTypeDef *f)
{
    (void)h;
    filter_writes++;
    if (filter_status != HAL_OK) return filter_status;      /* nothing written */
    hw_wide = (f->FilterID1 == 0x000u && f->FilterID2 == 0x7FFu);
    return HAL_OK;
}

static void reset(void)
{
    active = false;
    filter_err = false;
    restore_pending = false;
    can_sniff_reset();
    now_ms = 1000; filter_status = HAL_OK; hw_wide = false; filter_writes = 0;
}

static void feed(uint16_t id, uint8_t b0, uint32_t rx)
{
    uint8_t d[8] = { b0, 0, 0, 0, 0, 0, 0, 0 };
    can_sniff_feed(id, d, 8, rx);
}

/* ---------------------------------------------------------------- tests */
/* C3-02: leaving SNIFF while the HAL refuses the narrow filter used to
 * leave the wide filter installed with OBD polling resumed, and nothing
 * ever tried again — not even another "leave". */
static void t_failed_restore_is_retried(void)
{
    can_sniff_set_active(true);
    CHECK(active && hw_wide && !can_sniff_filter_error());
    filter_status = HAL_ERROR;                 /* controller mid-recovery */
    can_sniff_set_active(false);
    CHECK(!active && hw_wide && can_sniff_filter_error());
    filter_status = HAL_OK;                    /* healthy again */
    now_ms += 500u;
    can_sniff_set_active(false);               /* the UI keeps saying "off" */
    can_sniff_service();                       /* and the loop runs */
    CHECK(!hw_wide);                           /* narrow filter back */
    CHECK(!can_sniff_filter_error());
    CHECK(!can_sniff_restore_pending());
}

/* Retries are paced, not a busy loop, and stop once it worked. */
static void t_restore_retry_is_paced(void)
{
    can_sniff_set_active(true);
    filter_status = HAL_ERROR;
    can_sniff_set_active(false);
    int w0 = filter_writes;
    for (int i = 0; i < 10; i++) can_sniff_service();          /* same ms */
    CHECK(filter_writes - w0 <= 1);
    CHECK(can_sniff_restore_pending());
    for (int i = 0; i < 5; i++) { now_ms += SNIFF_RESTORE_RETRY_MS; can_sniff_service(); }
    CHECK(filter_writes - w0 >= 5);                            /* kept trying */
    filter_status = HAL_OK;
    now_ms += SNIFF_RESTORE_RETRY_MS;
    can_sniff_service();
    CHECK(!hw_wide && !can_sniff_restore_pending());
    int w1 = filter_writes;
    now_ms += 10u * SNIFF_RESTORE_RETRY_MS;
    can_sniff_service();
    CHECK(filter_writes == w1);                                /* done */
}

/* Entering: a refused wide filter is reported, the sniffer stays off. */
static void t_failed_enter_reports(void)
{
    filter_status = HAL_ERROR;
    CHECK(!can_sniff_set_active(true));
    CHECK(!active && !hw_wide && can_sniff_filter_error());
    filter_status = HAL_OK;
    CHECK(can_sniff_set_active(true));
    CHECK(active && hw_wide && !can_sniff_filter_error());
}

/* Entering again while a restore is pending takes the wide filter as is. */
static void t_enter_while_restore_pending(void)
{
    can_sniff_set_active(true);
    filter_status = HAL_ERROR;
    can_sniff_set_active(false);
    filter_status = HAL_OK;
    CHECK(can_sniff_set_active(true));
    CHECK(active && hw_wide && !can_sniff_restore_pending());
}

/* Inactive: frames are ignored entirely. */
static void t_inactive_ignores_frames(void)
{
    feed(0x123, 1, now_ms);
    CHECK(can_sniff_id_count() == 0);
}

/* Most recently changed first — by AGE, so it holds across the 32-bit
 * tick wrap (ranking by the raw stamp put a pre-wrap change on top). */
static void t_candidates_rank_by_age_across_wrap(void)
{
    can_sniff_set_active(true);
    feed(0x100, 1, 0xFFFFFE00u); feed(0x100, 2, 0xFFFFFF00u);   /* changed pre-wrap  */
    feed(0x200, 1, 0xFFFFFE00u); feed(0x200, 2, 0x00000010u);   /* changed post-wrap */
    now_ms = 0x00000020u;
    sniff_cand_t c[4];
    uint8_t n = can_sniff_candidates(c, 4);
    CHECK(n == 2);
    CHECK(n >= 1 && c[0].id == 0x200 && c[0].age_ms == 0x10u);
    CHECK(n >= 2 && c[1].id == 0x100 && c[1].age_ms == 0x120u);
}

/* A change stamped at tick 0 is still a change. */
static void t_change_at_tick_zero_counts(void)
{
    can_sniff_set_active(true);
    feed(0x300, 1, 0u); feed(0x300, 2, 0u);
    now_ms = 5u;
    sniff_cand_t c[2];
    CHECK(can_sniff_candidates(c, 2) == 1 && c[0].id == 0x300);
}

/* D06: ages come from the receive time passed in, not the drain time. */
static void t_age_from_receive_time(void)
{
    can_sniff_set_active(true);
    feed(0x400, 1, 1000u); feed(0x400, 2, 1100u);
    now_ms = 1500u;                                /* drained 400 ms later */
    sniff_cand_t c[1];
    CHECK(can_sniff_candidates(c, 1) == 1 && c[0].age_ms == 400u);
}

/* Table full: new IDs are not tracked, known ones still are; nothing
 * is evicted, and the fps count still sees every frame. */
static void t_table_full(void)
{
    can_sniff_set_active(true);
    for (uint16_t id = 0; id < SNIFF_MAX_IDS; id++) feed(id, 0, now_ms);
    CHECK(can_sniff_id_count() == SNIFF_MAX_IDS);
    feed(0x7AA, 0, now_ms);                           /* one too many */
    CHECK(can_sniff_id_count() == SNIFF_MAX_IDS);
    feed(5, 9, now_ms + 10u);                         /* known one changes */
    now_ms += 1000u;
    CHECK(can_sniff_fps() == SNIFF_MAX_IDS + 2u);
    now_ms += 1u;
    sniff_cand_t c[1];
    CHECK(can_sniff_candidates(c, 1) == 1 && c[0].id == 5);
}

/* Movers: widest span first; a free-running counter is excluded. */
static void t_movers(void)
{
    can_sniff_set_active(true);
    feed(0x500, 10, 1); feed(0x500, 90, 2);           /* span 80        */
    feed(0x501, 50, 1); feed(0x501, 60, 2);           /* span 10        */
    for (unsigned i = 0; i <= SNIFF_MOVER_MAX_CH + 1u; i++)
        feed(0x502, (uint8_t)i, 3u + i);              /* counter        */
    now_ms = 5000u;
    sniff_mover_t m[3];
    uint8_t n = can_sniff_movers(m, 3);
    CHECK(n == 2);
    CHECK(n >= 1 && m[0].id == 0x500 && m[0].vmin == 10 && m[0].vmax == 90);
    CHECK(n >= 2 && m[1].id == 0x501);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "failed_restore_is_retried",   t_failed_restore_is_retried },
        { "restore_retry_is_paced",      t_restore_retry_is_paced },
        { "failed_enter_reports",        t_failed_enter_reports },
        { "enter_while_restore_pending", t_enter_while_restore_pending },
        { "inactive_ignores_frames",     t_inactive_ignores_frames },
        { "candidates_rank_by_age_wrap", t_candidates_rank_by_age_across_wrap },
        { "change_at_tick_zero_counts",  t_change_at_tick_zero_counts },
        { "age_from_receive_time",       t_age_from_receive_time },
        { "table_full",                  t_table_full },
        { "movers",                      t_movers },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        reset();
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
