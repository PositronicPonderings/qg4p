/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_flash.h
 * @brief   A photosensitivity-SAFER screen flash: a gentle, rate-capped,
 *          self-ending pulse of the backlight, for "look at me!" moments.
 *
 * LAYER:   Public API (optional extra; include it yourself)
 * DEPENDS: qg_screen.h, qg_palette.h
 *
 *     #include "qg_flash.h"
 *
 *     static qg_flash_t crit_flash;                 // yours; zeroed (static)
 *     ...
 *     qg_flash_start(&crit_flash, &scr, NULL, now_ms);   // NULL = defaults
 *     while (game_running) {
 *         uint32_t now_ms = to_ms_since_boot(get_absolute_time());
 *         qg_flash_update(&crit_flash, now_ms);     // cheap; call every loop
 *         ... the rest of your game, which keeps running ...
 *     }
 *
 * SAFER, NOT SAFE
 * Flashing light can trigger seizures in people with photosensitive
 * epilepsy. This module is built to stay inside the W3C's guidance (WCAG 2.x,
 * success criterion 2.3.1, "Three Flashes or Below Threshold"), with margin.
 * That makes it SAFER. It does not make it SAFE: no piece of software can
 * promise that a flash won't trigger a seizure in a particular person, and
 * we are about a kilobyte of C on a microcontroller, not a neurologist.
 * Devices that flash should ALSO offer a setting that turns flashing off,
 * and respect it (just don't call qg_flash_start when it's off).
 *
 * WHAT IT GUARANTEES, WHATEVER IT'S ASKED FOR
 *   1. At most 2 flashes in any one-second window (WCAG allows 3; we keep one
 *      in our pocket). Ask for 10 a second and you get the cap, not an error.
 *      It holds however you cancel and restart, as long as you use the same
 *      qg_flash_t for the same screen.
 *   2. Smooth: brightness eases down and back up along a raised-cosine curve;
 *      no hard on/off steps, and no single update moves the backlight by
 *      more than QG_FLASH_MAX_STEP percentage points.
 *   3. It ends by itself: at most QG_FLASH_MAX_COUNT flashes per start.
 *   4. No saturated red: it refuses (QG_ERR_ARG) to flash a screen whose
 *      foreground, background or highlight colour is a saturated red
 *      (see qg_flash_is_saturated_red).
 *   5. Gentle by default: it dims to half the current brightness, not to
 *      black. You may choose the depth; the rate cap still applies.
 *   6. Never blocks: you call qg_flash_update() from your loop.
 *      Cancellable, and the brightness is restored exactly afterwards.
 *   7. Works the same on DIRECT and framebuffer (BUF8) screens, because it
 *      only touches the backlight: no pixel is read, written or re-sent.
 *
 * PAY FOR WHAT YOU USE
 * The code lives in qg_flash.c and is linked only if you call it. All the
 * state lives in a qg_flash_t that YOU own; nothing is added to qg_screen_t.
 */
#ifndef QG_FLASH_H
#define QG_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "qg_types.h"
#include "qg_screen.h"

/* ========================================================================== */
/*  The limits. These are deliberately NOT in qg_config.h: they aren't         */
/*  settings to tune, they're the promise this module makes.                  */
/* ========================================================================== */

/**
 * THE RULE everything else serves: a dip may not begin until this long after
 * the dip two before it ended. Why that's enough to keep to two flashes a
 * second is shown in qg_flash.c ("THE RATE ARITHMETIC"); the short version
 * is that any five changes in brightness always stretch from one dip to the
 * dip two after it, so they can't all fit inside one second.
 */
#define QG_FLASH_WINDOW_MS       1000u

/**
 * Shortest time one flash (dim, return, rest) may take, in milliseconds.
 * Ask for less and you get this. Each flash spends 3/8 of its period dimming,
 * 3/8 coming back and 1/4 resting, so a dip lasts 3/4 of a period, and
 * steady flashing obeys the rule above when 2 x period >= 3/4 x period +
 * 1000 ms: period >= 800 ms. At 800 or slower, the rule never has to step
 * in during a steady flash; it's there for restarts, cancels and slow loops.
 * (That's at most about 1.25 flashes a second, against WCAG's limit of 3.)
 */
#define QG_FLASH_MIN_PERIOD_MS   800u

/** Longest period accepted (slower requests are slowed to this): a flash
 *  that takes longer than four seconds has stopped being a flash and become
 *  a mood. Keeps the total time bounded too: 5 x 4 s = 20 s at most.        */
#define QG_FLASH_MAX_PERIOD_MS   4000u

/** Most flashes per qg_flash_start(). Attention is drawn by the second one;
 *  by the fifth it's just showing off.                                      */
#define QG_FLASH_MAX_COUNT       5u

/**
 * The flash's own clock never advances more than this per update, in ms.
 * If your loop is slow (a 72 ms framebuffer flush, say), the flash runs in
 * slow motion rather than in big jumps. Slowing down only ever helps: it lowers
 * the rate, and it keeps every step small.
 */
#define QG_FLASH_MAX_TICK_MS     20u

/**
 * The biggest change in brightness, in percentage points, that any single
 * qg_flash_update() makes. Worked out in qg_flash.c from the steepest part
 * of the curve: 100 x pi / (2 x 300 ms) x 20 ms = 10.5, plus one for
 * rounding to whole percent, rounded up. The tests hold the code to it.
 */
#define QG_FLASH_MAX_STEP        12u

/* The defaults, used for any config field left at 0 (or a NULL config). */
#define QG_FLASH_DEFAULT_PERIOD_MS 1000u  /**< One flash a second: calm.       */
#define QG_FLASH_DEFAULT_COUNT     3u     /**< Three: noticed, not nagging.    */
#define QG_FLASH_DEFAULT_DEPTH     50u    /**< Dim to half brightness, not off.*/

/* ========================================================================== */
/*  Types                                                                      */
/* ========================================================================== */

/**
 * What you'd like. Every field may be left at 0 for its default, and every
 * field is clamped to the limits above rather than refused: ask for the
 * moon and you get a gentle glow.
 */
typedef struct {
    uint16_t   period_ms; /**< Time per flash. 0 = 1000. Clamped to 800..4000. */
    uint8_t    count;     /**< Flashes. 0 = 3. Clamped to 1..5.                */
    uint8_t    depth;     /**< How far to dim, percent of the current
                               brightness. 0 = 50. 100 = all the way to dark
                               (allowed, still rate-capped; not recommended). */
    qg_color_t color;     /**< The colour of whatever you're drawing attention
                               to (your "CRITICAL!" banner), so it can be
                               checked for saturated red too. QG_DEFAULT =
                               nothing extra to check. (A zeroed config gives
                               0, QG_BLACK, which is never red: also fine.)   */
} qg_flash_config_t;

/**
 * One flash in progress (or not). YOURS: keep it static or global, start it
 * zeroed (static variables are), and reuse the same one for the same screen:
 * it remembers when its last two dips ended, which is how the rate cap holds
 * across cancels and back-to-back starts. Read-only for application code.
 */
typedef struct {
    qg_screen_t *scr;        /**< The screen being flashed.                    */
    uint32_t     t;          /**< The flash's own clock, ms.                   */
    uint32_t     dip_t;      /**< Flash clock time the current/next dip starts.*/
    uint32_t     last_ms;    /**< Your clock at the last update.               */
    uint32_t     dip_end[2]; /**< Your clock when the last two dips ended
                                  ([0] most recent). Kept between flashes.     */
    uint16_t     period;     /**< Period in use, ms (after clamping).          */
    uint16_t     ramp;       /**< Time to dim (and to return), ms.             */
    uint8_t      ends_known; /**< How many of dip_end[] are real (0..2).       */
    uint8_t      count;      /**< Dips to do in this flash.                    */
    uint8_t      done;       /**< Dips finished so far.                        */
    uint8_t      base;       /**< The brightness to come home to.              */
    uint8_t      amp;        /**< How many percentage points to dim by.        */
    uint8_t      last;       /**< The brightness we last set.                  */
    bool         in_dip;     /**< Part-way through a dip right now?            */
    bool         active;     /**< Flashing right now?                          */
} qg_flash_t;

/* ========================================================================== */
/*  Functions                                                                  */
/* ========================================================================== */

/**
 * Start flashing a screen. Returns at once; the flash happens as you call
 * qg_flash_update().
 *
 * @param f       your qg_flash_t (zeroed the first time)
 * @param scr     the screen; it needs a backlight pin
 * @param cfg     what you'd like, or NULL for the defaults
 * @param now_ms  your clock, in milliseconds (any clock that counts up;
 *                wrapping round after 49 days is fine)
 * @return QG_OK (including "nothing to do": the backlight is off or at 0 %,
 *         or this f is already flashing, in which case it carries on);
 *         QG_ERR_ARG for NULL pointers, or if the screen's foreground or
 *         background colour, or cfg->color, is a saturated red;
 *         QG_ERR_UNSUPPORTED if the screen has no backlight pin.
 *
 * If this same f flashed only moments ago, the first dip may wait a
 * little (never more than a second), so that starts in quick succession can't
 * add up to more than the cap.
 */
qg_err_t qg_flash_start(qg_flash_t *f, qg_screen_t *scr,
                        const qg_flash_config_t *cfg, uint32_t now_ms);

/**
 * Move the flash along. Call it every time round your main loop (any rate;
 * 50 per second or more looks smoothest). It sets the backlight and returns.
 *
 * @return true while the flash is still running; false once it's over and
 *         the brightness is back exactly where it started.
 *
 * If something else changes the screen's brightness while a flash is
 * running (qg_screen_set_brightness), the flash steps aside: it stops at
 * once and leaves your new brightness alone.
 */
bool qg_flash_update(qg_flash_t *f, uint32_t now_ms);

/**
 * Stop a flash early.
 *
 * @param now_ms   your clock, as for qg_flash_update()
 * @param at_once  false: ease back up to the starting brightness along the
 *                 same smooth curve (keep calling qg_flash_update() until it
 *                 returns false; that takes at most one ramp, 300..1500 ms).
 *                 true:  restore the starting brightness right now, in one
 *                 step. For when the screen is about to be switched off or
 *                 the device is going to sleep; it's the "up" half of a dip
 *                 already under way, so it doesn't add a flash, but it isn't
 *                 smooth, so prefer false.
 * Harmless to call on a flash that isn't running.
 */
void qg_flash_cancel(qg_flash_t *f, uint32_t now_ms, bool at_once);

/** True while a flash is running (the same answer qg_flash_update gave). */
static inline bool qg_flash_active(const qg_flash_t *f) { return f != NULL && f->active; }

/**
 * Is this palette entry a saturated red, by WCAG's working definition
 * R / (R + G + B) >= 0.8 (8-bit sRGB values from the screen's palette)?
 * qg_flash_start uses this; it's public so you can choose colours that pass.
 * QG_RED (170,0,0) and QG_LIGHTRED (255,85,85: 0.6) show the line: the
 * first is refused, the second is fine. Black, QG_DEFAULT, QG_NONE and
 * QG_TRANSPARENT are never red.
 */
bool qg_flash_is_saturated_red(const qg_screen_t *scr, qg_color_t color);

#endif /* QG_FLASH_H */
