#ifndef INPUT_H
#define INPUT_H

/* Button gestures and the day/night input, as pure logic (no HAL), so the
 * host tests run them (tests/host/test_input.c). app_main.c samples the pins,
 * counts presses in the EXTI handlers and maps the events to actions.
 *
 * Buttons (docs/future/build-hardware.md §6c): Toyota LC100 switch blank with
 * three momentary buttons, dry contacts to ground, 1 k pull-up, so pressed =
 * LOW; the on-board KEY (PC13, pressed = HIGH) works as a second ▶.
 *
 * A press can start and end inside one superloop iteration (a full-screen
 * flush stalls it ~0.4 s), so the EXTI handler counts debounced press edges
 * and btn_step() replays those as taps — the scheme KEY has used since
 * stage A, now shared by all four buttons. */

#include <stdbool.h>
#include <stdint.h>

#define BTN_DEBOUNCE_MS 30u          /* edges closer than this are bounce     */
#define BTN_MAX_BURST    4u          /* cap on taps replayed in one go         */

typedef struct {
    uint32_t hold_ms;                /* hold gesture after this; 0 = none      */
    /* state */
    bool     prev;                   /* pressed at the last sample             */
    bool     hold_done;              /* this press already gave its hold       */
    uint32_t down_t;                 /* tick the current press began           */
    uint32_t last_edge;              /* tick of the last accepted edge         */
    uint8_t  seen;                   /* EXTI presses already turned into taps  */
} btn_t;

typedef struct {
    uint8_t taps;                    /* short presses completed                */
    bool    hold;                    /* held for hold_ms (once per press)      */
} btn_out_t;

/* One sample of a button: pressed = its level now; presses / press_t = the
 * EXTI's free-running count of debounced presses and the tick of the latest. */
btn_out_t btn_step(btn_t *b, bool pressed, uint8_t presses, uint32_t press_t, uint32_t now);

/* Day / night from the parking-light input (BC547 pulls PE15 LOW while the
 * dash illumination is on). Any LOW within DN_PWM_HOLD_MS counts as "on", so
 * a PWM-dimmed illumination line still reads as night; a change is taken only
 * after it has lasted DN_SETTLE_MS, so the backlight does not flicker. */
#define DN_PWM_HOLD_MS 100u
#define DN_SETTLE_MS   500u

typedef struct {
    bool     night;                  /* settled state (false at power-up = day) */
    bool     pending;                /* candidate state ...                    */
    uint32_t pending_t;              /* ... seen continuously since this tick   */
    bool     ever_low;
    uint32_t last_low;
} daynight_t;

bool daynight_step(daynight_t *d, bool pin_low, uint32_t now);

#endif /* INPUT_H */
