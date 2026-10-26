/* Button gestures and day/night filter — see input.h. */

#include "input.h"

btn_out_t btn_step(btn_t *b, bool pressed, uint8_t presses, uint32_t press_t, uint32_t now)
{
    btn_out_t o = { 0, false };

    /* The level path took a press at the previous sample; its edge fell
     * between the count snapshot and the level read, so the EXTI's count of
     * that same press arrives only now. All presses counted within the
     * debounce window of that press's start are that press, not more taps
     * (a real second press needs a release and 30 ms of quiet first). */
    if (b->late_ok) {
        b->late_ok = false;
        if (presses != b->seen && (uint32_t)(press_t - b->down_t) < BTN_DEBOUNCE_MS)
            b->seen = presses;
    }

    /* Presses the EXTI counted that have not been handled yet. */
    uint8_t unseen = (uint8_t)(presses - b->seen);
    if (unseen > BTN_MAX_BURST) unseen = BTN_MAX_BURST;

    /* Whole taps between two samples: the level never looked pressed, but the
     * EXTI counted the presses. Wait out the debounce window after the latest
     * one, so a sample that landed on press bounce is not taken as a release. */
    if (unseen && !pressed && !b->prev && (now - press_t) >= BTN_DEBOUNCE_MS) {
        o.taps  = unseen;
        b->seen = presses;
        unseen  = 0;
    }

    /* Edge with time-based debounce. A tap fires on the RELEASE edge however
     * briefly the button was held; an edge within BTN_DEBOUNCE_MS of the last
     * accepted one is ignored (prev stays, so it is looked at again later). A
     * press seen here by level is the newest counted press; any counted before
     * it were whole taps inside a stalled iteration. */
    if (pressed != b->prev && (now - b->last_edge) >= BTN_DEBOUNCE_MS) {
        b->last_edge = now;
        if (pressed) {
            if (unseen > 1u) o.taps = (uint8_t)(o.taps + unseen - 1u);
            b->seen      = presses;
            b->down_t    = now;
            b->hold_done = false;
            b->late_ok   = true;
        } else if (!b->hold_done) {
            o.taps++;
        }
        b->prev = pressed;
    }

    /* Hold: by the live level, once per press; the release after it is no tap. */
    if (b->hold_ms && pressed && b->prev && !b->hold_done && (now - b->down_t) >= b->hold_ms) {
        b->hold_done = true;
        o.hold = true;
    }
    return o;
}

void daynight_init(daynight_t *d, bool pin_low, uint32_t now)
{
    d->night     = pin_low;
    d->pending   = pin_low;
    d->pending_t = now;
    d->ever_low  = pin_low;
    d->last_low  = now;
}

bool daynight_step(daynight_t *d, bool pin_low, uint32_t now)
{
    if (pin_low) { d->ever_low = true; d->last_low = now; }
    bool raw = d->ever_low && (now - d->last_low) < DN_PWM_HOLD_MS;
    if (raw != d->pending) {
        d->pending   = raw;
        d->pending_t = now;
    } else if (raw != d->night && (now - d->pending_t) >= DN_SETTLE_MS) {
        d->night = raw;
    }
    return d->night;
}
