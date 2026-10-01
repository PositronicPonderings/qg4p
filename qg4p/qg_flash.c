/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_flash.c
 * @brief   The photosensitivity-safer flash: a smooth, rate-capped,
 *          self-ending dip of the backlight. See qg_flash.h for the promise.
 *
 * LAYER:   Public API (optional extra)
 * DEPENDS: qg_flash.h, qg_screen.h (brightness), qg_palette.h (the red check)
 *
 * ---------------------------------------------------------------------------
 *  WHY THIS FILE EXISTS, AND WHY IT IS SO FUSSY
 * ---------------------------------------------------------------------------
 * Every game, sooner or later, wants to flash the screen. You rolled a
 * natural 20! The reactor is going critical! You have mail! And the oldest
 * way to do it, the one every one of us has written at two in the morning,
 * is this:
 *
 *     for (int i = 0; i < 10; i++) { backlight(off); sleep_ms(50);
 *                                    backlight(on);  sleep_ms(50); }
 *
 * Ten hard flashes a second, full black to full white. It's exactly the
 * pattern that can trigger a seizure in someone with photosensitive
 * epilepsy, and it froze the rest of the game for a second while it did it.
 * Two bugs for the price of one, and only one of them is funny.
 *
 * The W3C's Web Content Accessibility Guidelines (WCAG 2.x) give the rule
 * that broadcasters and web pages use, success criterion 2.3.1, "Three
 * Flashes or Below Threshold":
 *
 *     https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold
 *
 * In short: nothing may flash more than three times in any one-second period
 * unless the flash is too small or too faint to matter. A FLASH is a PAIR of
 * opposing changes in brightness: down then up, or up then down. A "general
 * flash" is a pair big enough to count (a swing of 10 % or more of maximum
 * relative luminance, with the darker state below 0.80); a "red flash" is
 * any pair of opposing changes involving a saturated red.
 *
 * WCAG also excuses flashes that cover only a small part of what you're
 * looking at (25 % of any 10-degree patch of your visual field). That
 * exception is no help to us whatsoever. A 2 to 3.5 inch screen held at
 * arm's length IS your 10-degree patch, more or less, and we're flashing all
 * of it. So the only promise we can keep is the rate. Everything below is in
 * service of that, plus a few extra margins because we're the sort of people
 * who wear a belt AND braces. (The braces are also a belt. We're nervous.)
 *
 * Asimov gave his robots Three Laws, the first being that a robot may not
 * injure a human being. This little module has the same first law, minus
 * the positronic brain; it makes do with a Cortex-M33 and a lookup table.
 *
 * ---------------------------------------------------------------------------
 *  THE MECHANISM: WHY THE BACKLIGHT?
 * ---------------------------------------------------------------------------
 * There are three ways to make a whole screen pulse:
 *
 *   1. Redraw it brighter and darker. On a 320x480 framebuffer screen a full
 *      redraw-and-flush takes about 72 ms, so a smooth fade would eat the
 *      game whole. On a DIRECT screen we can't even do that: there's no copy
 *      of the picture to redraw from.
 *   2. Pulse the palette. Free on paper, but on a DIRECT screen pixels that
 *      are already on the glass don't care what the palette says now, and on
 *      a framebuffer screen a palette change re-sends every pixel (72 ms).
 *   3. Pulse the backlight. One PWM register write per step. Zero cost per
 *      pixel, identical on DIRECT and framebuffer screens, and nothing on the
 *      screen is touched, so "restore the screen exactly" just means putting
 *      one number back. Also: an LED backlight's light output is close to
 *      proportional to its PWM duty, and WCAG's "relative luminance" is
 *      linear light, so a percentage of brightness is very nearly a
 *      percentage of luminance. The maths below gets to be honest maths.
 *
 * Number 3, then. Its one limitation: the screen needs a backlight pin
 * (bl_pin). A backlight wired straight to 3.3 V can't be dimmed by anything
 * short of a pair of scissors, and we don't recommend those either.
 *
 * ---------------------------------------------------------------------------
 *  THE SHAPE OF ONE FLASH
 * ---------------------------------------------------------------------------
 *
 *   brightness
 *     base  ‾‾‾\                 /‾‾‾‾‾‾‾‾‾\                 /‾‾‾‾‾‾‾‾
 *               \               /           \               /
 *                \_           _/             \_           _/
 *     base-amp     ‾‾‾‾‾‾‾‾‾‾‾                 ‾‾‾‾‾‾‾‾‾‾‾
 *           |<- ramp ->|<- ramp ->|<- rest ->|
 *           |<------------ period ---------->|
 *
 *   ramp = 3/8 of the period (dimming), ramp again (returning), and the
 *   last 1/4 is a rest at full brightness. The curve within each ramp is a
 *   raised cosine, (1 - cos(pi x)) / 2: it starts and ends with zero slope,
 *   so there's no "edge" for the eye to catch. It's the same shape audio
 *   engineers use to fade a sound in without a click. Light, sound: it's all
 *   just things that hurt when you switch them on too fast.
 *
 * ---------------------------------------------------------------------------
 *  THE RATE ARITHMETIC (THE ONE RULE)
 * ---------------------------------------------------------------------------
 * We count the strict way. A "change" is one ramp, down or up. It counts as
 * being in a one-second window if any part of it so much as touches the
 * window, and we time it pessimistically: it "starts" at the update BEFORE
 * its first step (it might have begun any time after that) and "ends" at
 * its last step. Two flashes are
 * four changes, so the promise is: no window ever touches five.
 *
 * Every dip is two changes, down then up. Any five changes in a row are
 * therefore either
 *
 *     up(k)   down(k+1) up(k+1) down(k+2) up(k+2)      or
 *     down(k) up(k)     down(k+1) up(k+1) down(k+2)
 *
 * and either way the first ends no later than dip k ends, and the fifth
 * starts no earlier than dip k+2 starts. So if
 *
 *     a dip never starts until 1000 ms after the dip two before it ended
 *
 * then no one-second window can touch all five. That's the whole proof, and
 * it's the whole rule: it's checked, in real time on your clock, before
 * every dip. It doesn't care how long the dips are, whether one was cut
 * short by a cancel, how slow or jittery your loop is, or how often you
 * restart (the struct remembers its last two dips between flashes). We
 * like proofs that fit on a napkin. We've been burned by the other kind.
 *
 * The periods are chosen so the rule doesn't have to interrupt a steady
 * flash: a dip lasts 3/4 of a period, so steady flashing obeys it when
 * 2 x period >= 3/4 x period + 1000, which is period >= 800 ms. Faster
 * requests are slowed to 800. The rule stays on guard regardless; it's the
 * guarantee, the period is just good manners.
 *
 * ---------------------------------------------------------------------------
 *  TIME DILATION (THE SLOW-LOOP RULE)
 * ---------------------------------------------------------------------------
 * The flash keeps its own clock, f->t, which advances by the time since the
 * last update BUT never by more than 20 ms at once. If your loop is quick
 * (every 10 ms, say), the flash runs in real time. If your loop is slow
 * (a 72 ms framebuffer flush), the flash runs in slow motion: each update
 * moves 20 ms along the curve. Like a starship near light speed, the flash
 * experiences less time than you do. Unlike a starship, this is the better
 * direction to be wrong in: slower flashes are fewer flashes, and smaller
 * clock steps are smaller brightness steps. That's how QG_FLASH_MAX_STEP
 * holds no matter how slowly you call us.
 *
 * QG_FLASH_MAX_STEP itself: the steepest point of a raised cosine of height
 * A over a ramp of r ms has slope A x pi / (2 r) per ms. The worst case is
 * A = 100 (dimming from 100 % to 0 %), r = 300 ms (the shortest ramp), over
 * 20 ms: 100 x 3.1416 / 600 x 20 = 10.47 points. Rounding to whole percent
 * can add one more. Hence 12, with a little to spare. At the default depth
 * (50 %) and period (1000 ms, ramp 375 ms) a 20 ms step is at most about
 * 4.2 points.
 */
#include <stddef.h>
#include "qg_flash.h"
#include "qg_palette.h"

/* ========================================================================== */
/*  The curve                                                                 */
/* ========================================================================== */

/* (1 - cos(pi * i / 32)) / 2, scaled to 0..65535, for i = 0..32. A lookup
 * table because the Pico 2's FPU could do cosf() perfectly well, but then
 * every program that flashed would also carry the maths library, and this
 * file's whole personality is "you only pay for what you use". 66 bytes.
 * Linear interpolation between entries is within 0.1 % of the true curve,
 * which is below the resolution of a 1 %-step PWM, which is in turn below
 * the resolution of a human eyeball at 3 a.m. Good enough three times over.
 * (Interpolation can't make a step steeper than the curve's steepest point,
 * either: a straight line between two points on a smooth curve never is.) */
static const uint16_t s_rcos[33] = {
        0,   158,   630,  1411,  2494,  3869,  5522,  7438,
     9597, 11980, 14563, 17321, 20228, 23256, 26375, 29556,
    32767, 35979, 39160, 42279, 45307, 48214, 50972, 53555,
    55938, 58097, 60013, 61666, 63041, 64124, 64905, 65377,
    65535,
};

/* The raised cosine at x = num / den (0 <= num <= den), as 0..65535.        */
static uint32_t ease(uint32_t num, uint32_t den)
{
    /* Position in 1/256ths of a table step. num <= 1500 and 32 x 256 = 8192,
     * so this stays far below 2^32 even before we worry about it.          */
    uint32_t q    = num * 32u * 256u / den;
    uint32_t i    = q >> 8;
    uint32_t frac = q & 255u;
    if (i >= 32u) {
        return s_rcos[32];
    }
    return s_rcos[i] + (((uint32_t)(s_rcos[i + 1] - s_rcos[i]) * frac) >> 8);
}

/* ========================================================================== */
/*  The red check                                                             */
/* ========================================================================== */

bool qg_flash_is_saturated_red(const qg_screen_t *scr, qg_color_t color)
{
    /* QG_TRANSPARENT (255), QG_DEFAULT (0x100) and QG_NONE (0xFFFF) aren't
     * colours anyone sees, so they can't be red. Philosophically debatable;
     * practically fine.                                                    */
    if (scr == NULL || color >= QG_TRANSPARENT) {
        return false;
    }
    uint8_t r, g, b;
    qg_palette_get(scr->palette, color, &r, &g, &b);
    uint32_t sum = (uint32_t)r + g + b;

    /* WCAG's working definition of "saturated red": R / (R + G + B) >= 0.8.
     * In whole numbers, with no division and no floating point:
     *     5 R >= 4 (R + G + B)
     * Black (0, 0, 0) is 0/0, which is not red, it's just dark.
     *
     * WHY RED GETS ITS OWN RULE: the eye's response to deep red is its own
     * hazard, separate from plain brightness. The best-known incident was a
     * 1997 cartoon episode in Japan whose red/blue flashing sent hundreds of
     * children to hospital. Red flashes are the ones the guidelines are
     * strictest about, so we simply don't do them.                         */
    return sum > 0 && 5u * r >= 4u * sum;
}

/* ========================================================================== */
/*  Start, update, cancel                                                     */
/* ========================================================================== */

/* Remember that a dip has just ended, at `now_ms` on your clock.         */
static void dip_ended(qg_flash_t *f, uint32_t now_ms)
{
    f->dip_end[1] = f->dip_end[0];
    f->dip_end[0] = now_ms;
    if (f->ends_known < 2) {
        f->ends_known++;
    }
    f->in_dip = false;
    f->done++;
}

/* THE RULE: may a dip start, given that the update before this one was at
 * `prev_ms`? (The update before, not this one: the first step of the dip
 * may land on this update, and we time changes from the update before.)  */
static bool dip_allowed(const qg_flash_t *f, uint32_t prev_ms)
{
    if (f->ends_known < 2) {
        return true;                     /* fewer than two dips ever: fine */
    }
    /* How long still to wait, signed, so the clock wrapping is harmless.
     * A real answer is never more than 1000 ms (the dip ended in the past).
     * Anything bigger means the struct wasn't zeroed and dip_end[] is junk:
     * we let it go rather than wait for a junk deadline that might be weeks
     * away. (Waiting forever would be very cautious and very silly.)           */
    int32_t wait = (int32_t)(f->dip_end[1] + QG_FLASH_WINDOW_MS - prev_ms);
    return wait <= 0 || wait > (int32_t)QG_FLASH_WINDOW_MS;
}

/* Put everything back and stop.                                          */
static void finish(qg_flash_t *f, bool restore)
{
    if (restore && f->last != f->base) {
        qg_screen_set_brightness(f->scr, f->base);
        f->last = f->base;
    }
    f->in_dip = false;
    f->active = false;
}

qg_err_t qg_flash_start(qg_flash_t *f, qg_screen_t *scr,
                        const qg_flash_config_t *cfg, uint32_t now_ms)
{
    if (f == NULL || scr == NULL) {
        return QG_ERR_ARG;
    }
    if (f->active) {
        /* Already flashing. The flash in progress is attention enough;
         * stacking another on top is how you get ten flashes a second.
         * Same screen: carry on, no harm done. Another screen: one
         * qg_flash_t per screen, please (see qg_flash.h).                  */
        return (f->scr == scr) ? QG_OK : QG_ERR_ARG;
    }
    if (scr->cfg.bl_pin < 0) {
        return QG_ERR_UNSUPPORTED;     /* no backlight pin, nothing to dim  */
    }

    /* Requirement: no saturated red. Dimming the backlight dims every colour
     * on the glass at once, so the "colours involved" are what's on the
     * screen. We can't afford to look at every pixel (that's the point of
     * using the backlight), so we check the colours that cover most of it:
     * the background (what qg_cls fills with), the foreground (text and
     * lines), and the colour you tell us you're flashing to highlight. We
     * REFUSE rather than substitute: we don't draw anything, so there's no
     * colour of ours to swap, and silently repainting your banner would be
     * rude. Pick yellow or white; they're louder anyway.                   */
    if (qg_flash_is_saturated_red(scr, scr->bg_color) ||
        qg_flash_is_saturated_red(scr, scr->fg_color) ||
        (cfg != NULL && qg_flash_is_saturated_red(scr, cfg->color))) {
        return QG_ERR_ARG;
    }

    /* Defaults for zeros, then clamp. Clamping rather than refusing is the
     * point: "the easy way is also the safer way". Ask for ten flashes a
     * second and you get the cap; ask for a thousand flashes and you get
     * five, then peace and quiet.                                          */
    uint32_t period = (cfg && cfg->period_ms) ? cfg->period_ms : QG_FLASH_DEFAULT_PERIOD_MS;
    uint32_t count  = (cfg && cfg->count)     ? cfg->count     : QG_FLASH_DEFAULT_COUNT;
    uint32_t depth  = (cfg && cfg->depth)     ? cfg->depth     : QG_FLASH_DEFAULT_DEPTH;
    if (period < QG_FLASH_MIN_PERIOD_MS) period = QG_FLASH_MIN_PERIOD_MS;
    if (period > QG_FLASH_MAX_PERIOD_MS) period = QG_FLASH_MAX_PERIOD_MS;
    if (count  > QG_FLASH_MAX_COUNT)     count  = QG_FLASH_MAX_COUNT;
    if (depth  > 100u)                   depth  = 100u;

    /* Nothing to flash: a backlight that's off (or at 0 %) can't get any
     * darker, and we're not about to switch it ON to flash it. That's not
     * an error, it's just a very calm screen.                              */
    uint8_t base = qg_screen_get_brightness(scr);
    uint8_t amp  = (uint8_t)(((uint32_t)base * depth + 50u) / 100u);
    if (!scr->bl_on || amp == 0) {
        return QG_OK;
    }

    /* dip_end[] and ends_known are left alone on purpose: they're this
     * struct's memory of its last flash, and THE RULE needs them.          */
    f->scr     = scr;
    f->period  = (uint16_t)period;
    f->ramp    = (uint16_t)(period * 3u / 8u);
    f->base    = base;
    f->amp     = amp;
    f->last    = base;
    f->count   = (uint8_t)count;
    f->done    = 0;
    f->t       = 0;
    f->dip_t   = 0;                    /* the first dip, as soon as allowed */
    f->in_dip  = false;
    f->last_ms = now_ms;
    f->active  = true;
    return QG_OK;
}

bool qg_flash_update(qg_flash_t *f, uint32_t now_ms)
{
    if (f == NULL || !f->active) {
        return false;
    }

    /* Did someone else touch the brightness since we last did? Then they
     * have opinions, and they outrank us. We stop, and leave their setting
     * alone: "restore exactly" would mean undoing what they just did.     */
    if (qg_screen_get_brightness(f->scr) != f->last) {
        if (f->in_dip) {
            dip_ended(f, now_ms);      /* it ended, just not our way        */
        }
        finish(f, false);
        return false;
    }

    /* Advance the flash's clock, at most 20 ms per call (time dilation;
     * see the top of the file). Unsigned subtraction copes with your clock
     * wrapping round, which a 32-bit millisecond clock does every 49.7 days.
     * If your game has been running that long, congratulations, and please
     * go outside.                                                          */
    uint32_t prev_ms = f->last_ms;
    uint32_t dt      = now_ms - prev_ms;
    f->last_ms = now_ms;
    if (dt > QG_FLASH_MAX_TICK_MS) {
        dt = QG_FLASH_MAX_TICK_MS;
    }
    uint32_t t = f->t + dt;

    /* About to start a dip? Only if THE RULE allows; otherwise wait at the
     * door, at full brightness, and ask again next time. Like a bouncer,
     * but for photons.                                                     */
    if (!f->in_dip && t > f->dip_t) {
        if (dip_allowed(f, prev_ms)) {
            f->in_dip = true;
        } else {
            t = f->dip_t;
        }
    }
    f->t = t;

    if (f->in_dip && t >= f->dip_t + 2u * f->ramp) {
        dip_ended(f, now_ms);          /* back at the top                   */
        f->dip_t += f->period;         /* the next one, after a rest        */
        if (f->done >= f->count) {
            finish(f, true);           /* back exactly where we started     */
            return false;
        }
    }

    /* Where the curve says we should be. While resting or waiting at the
     * door, that's simply full (starting) brightness.                      */
    uint8_t b = f->base;
    if (f->in_dip) {
        uint32_t p = t - f->dip_t;     /* how far into this dip             */
        uint32_t w = (p < f->ramp) ? ease(p, f->ramp)                 /* down */
                                   : ease(2u * f->ramp - p, f->ramp); /* up   */
        /* Round to the nearest whole percent. Each ramp is monotonic, and
         * rounding keeps it monotonic, so a ramp can never wobble and sneak
         * in an extra "opposing change". (We checked. Then the tests
         * checked us. Trust, but verify; then verify the verifier.)        */
        b = (uint8_t)(f->base - ((uint32_t)f->amp * w + 32767u) / 65535u);
    }
    if (b != f->last) {                /* only write when it changes        */
        qg_screen_set_brightness(f->scr, b);
        f->last = b;
    }
    return true;
}

void qg_flash_cancel(qg_flash_t *f, uint32_t now_ms, bool at_once)
{
    if (f == NULL || !f->active) {
        return;
    }
    if (!f->in_dip) {
        finish(f, true);               /* resting: already at full          */
        return;
    }
    if (at_once) {
        /* Put it back in one go (unless someone else has changed the
         * brightness meanwhile). See qg_flash.h for why this is the
         * exception. The dip ends now, as far as THE RULE is concerned.    */
        bool ours = qg_screen_get_brightness(f->scr) == f->last;
        dip_ended(f, now_ms);
        finish(f, ours);
        return;
    }

    uint32_t p = f->t - f->dip_t;
    if (p < f->ramp) {
        /* Part-way down. Jump to the mirror-image point on the way back up:
         * same brightness, same slope, opposite direction. The dip simply
         * turns round, like a pilot who's remembered the oven's on.        */
        f->t = f->dip_t + 2u * f->ramp - p;
    }
    /* Either way, this dip is the last: it's over at the top of the ramp.  */
    f->count = (uint8_t)(f->done + 1u);
}
