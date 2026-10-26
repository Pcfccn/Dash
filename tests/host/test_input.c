/* Host-side tests for Core/Src/input.c: button gestures (taps by level and by
 * EXTI counts, bounce, holds, burst cap) and the day/night filter. */
#include <stdio.h>
#include <string.h>

#include "../../Core/Src/input.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static btn_t   B;
static uint8_t presses;              /* what the EXTI would have counted */
static uint32_t press_t;
static int     taps, holds;

/* Sample the button every 10 ms for ms, at the given level. */
static void hold_level(bool pressed, uint32_t *t, uint32_t ms)
{
    for (uint32_t k = 0; k < ms; k += 10) {
        *t += 10;
        btn_out_t o = btn_step(&B, pressed, presses, press_t, *t);
        taps += o.taps;
        holds += o.hold;
    }
}
static void exti_press(uint32_t t) { presses++; press_t = t; }

static void reset(uint32_t hold_ms)
{
    memset(&B, 0, sizeof B);
    B.hold_ms = hold_ms;
    presses = 0; press_t = 0; taps = 0; holds = 0;
}

static void t_tap_on_release(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 100);
    exti_press(t + 1); hold_level(true, &t, 150);
    CHECK(taps == 0);                                /* not on the press        */
    hold_level(false, &t, 100);
    CHECK(taps == 1 && holds == 0);
}

static void t_bounce_is_one_tap(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 100);
    /* bounce inside the 30 ms window, on the press and on the release */
    exti_press(t + 1);
    hold_level(true, &t, 10); hold_level(false, &t, 10);
    hold_level(true, &t, 200);
    hold_level(false, &t, 10); hold_level(true, &t, 10);
    hold_level(false, &t, 100);
    CHECK(taps == 1);
}

static void t_hold_once_no_tap(void)
{
    uint32_t t = 1000;
    reset(2000);
    hold_level(false, &t, 50);
    exti_press(t + 1); hold_level(true, &t, 2500);
    CHECK(holds == 1);                               /* once per press          */
    hold_level(true, &t, 2000);
    CHECK(holds == 1);
    hold_level(false, &t, 100);
    CHECK(taps == 0);                                /* the release is no tap   */
}

static void t_no_hold_gesture(void)
{
    uint32_t t = 1000;
    reset(0);                                        /* ◀: taps only            */
    hold_level(false, &t, 50);
    exti_press(t + 1); hold_level(true, &t, 5000);
    hold_level(false, &t, 100);
    CHECK(holds == 0 && taps == 1);
}

static void t_taps_inside_a_stall(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 50);
    /* two whole presses between two samples: only the EXTI saw them */
    exti_press(t + 5); exti_press(t + 120);
    t += 400;                                        /* the stalled iteration   */
    btn_out_t o = btn_step(&B, false, presses, press_t, t);
    CHECK(o.taps == 2);
    hold_level(false, &t, 100);
    CHECK(taps == 0);                                /* not counted again       */
}

static void t_press_seen_after_whole_taps(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 50);
    exti_press(t + 5); exti_press(t + 100); exti_press(t + 300);
    t += 350;
    btn_out_t o = btn_step(&B, true, presses, press_t, t);   /* 3rd one still down */
    CHECK(o.taps == 2);                              /* the first two           */
    hold_level(false, &t, 100);
    CHECK(taps == 1);                                /* and the third on release */
}

static void t_burst_capped(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 50);
    for (int i = 0; i < 10; i++) exti_press(t + 1 + (uint32_t)i);
    t += 400;
    btn_out_t o = btn_step(&B, false, presses, press_t, t);
    CHECK(o.taps == BTN_MAX_BURST);
}

/* Review C8: the press edge falls between the count snapshot and the level
 * read, so the level path takes the press first and the EXTI's count of it
 * arrives one sample later. That used to be replayed as a second tap after
 * the release. */
static void t_late_count_is_the_same_press(void)
{
    uint32_t t = 1000;
    reset(3000);
    hold_level(false, &t, 100);
    t += 10;
    btn_out_t o = btn_step(&B, true, presses, press_t, t);   /* level: pressed, count old */
    taps += o.taps;
    exti_press(t);                                          /* ISR ran just after */
    hold_level(true, &t, 120);
    hold_level(false, &t, 100);
    CHECK(taps == 1);
    /* a real second press later still counts */
    exti_press(t + 1); hold_level(true, &t, 100);
    hold_level(false, &t, 100);
    CHECK(taps == 2);
}

/* Review C8: the first profile comes from the pin, not "day" for 0.5 s. */
static void t_daynight_init_from_pin(void)
{
    daynight_t d;
    memset(&d, 0, sizeof d);
    daynight_init(&d, true, 5000);
    CHECK(d.night);
    CHECK(daynight_step(&d, false, 5010));                  /* a PWM off phase   */
    CHECK(daynight_step(&d, true, 5020));
    daynight_init(&d, false, 9000);
    CHECK(!d.night && !daynight_step(&d, false, 9010));
}

static void t_daynight(void)
{
    daynight_t d;
    memset(&d, 0, sizeof d);
    uint32_t t = 1000;
    CHECK(!daynight_step(&d, false, t));             /* day at power-up         */
    for (int i = 0; i < 40; i++) daynight_step(&d, true, t += 10);   /* 400 ms */
    CHECK(!d.night);                                 /* not settled yet         */
    for (int i = 0; i < 20; i++) daynight_step(&d, true, t += 10);
    CHECK(d.night);                                  /* after 500 ms            */
    /* PWM-dimmed illumination: low 1 sample in 5 (every 50 ms) -> still night */
    for (int i = 0; i < 200; i++) daynight_step(&d, (i % 5) == 0, t += 10);
    CHECK(d.night);
    /* lights off: high from now on -> day after 100 + 500 ms */
    for (int i = 0; i < 50; i++) daynight_step(&d, false, t += 10);
    CHECK(d.night);
    for (int i = 0; i < 60; i++) daynight_step(&d, false, t += 10);
    CHECK(!d.night);
}

static void t_daynight_glitch_ignored(void)
{
    daynight_t d;
    memset(&d, 0, sizeof d);
    uint32_t t = 1000;
    daynight_step(&d, true, t += 10);                /* one low spike           */
    for (int i = 0; i < 200; i++) daynight_step(&d, false, t += 10);
    CHECK(!d.night);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "tap_on_release",            t_tap_on_release },
        { "bounce_is_one_tap",         t_bounce_is_one_tap },
        { "hold_once_no_tap",          t_hold_once_no_tap },
        { "no_hold_gesture",           t_no_hold_gesture },
        { "taps_inside_a_stall",       t_taps_inside_a_stall },
        { "press_seen_after_taps",     t_press_seen_after_whole_taps },
        { "burst_capped",              t_burst_capped },
        { "late_count_same_press",     t_late_count_is_the_same_press },
        { "daynight_init_from_pin",    t_daynight_init_from_pin },
        { "daynight",                  t_daynight },
        { "daynight_glitch_ignored",   t_daynight_glitch_ignored },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
