/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m3_demo.c
 * @brief   Milestone 3 test: relative (percentage) coordinates, and the
 *          faster arcs.
 *
 * Built by tests/hardware/CMakeLists.txt as its own target (qg4p_m3.uf2).
 *
 * PAGES
 *   1  Layout     A mock dice-roller screen drawn ONLY with _pct calls, on
 *                 both screens, upright and then turned 90 degrees. The same
 *                 code fits itself to every screen size and orientation.
 *   2  Gauge      The M2 gauge, timed with no pauses. Compare the "per slice"
 *                 figure with M2's (roughly 12 ms on screen A).
 *   3  Dial       A lock-style dial: a marker spins around a ring for a few
 *                 seconds on both screens, reporting frames per second.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "demo_setup.h"

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };
static const char   *const names[2]   = { "A", "B" };

static void print_ms(const char *label, uint64_t us)
{
    printf("  %-6s %lu.%lu ms\n", label,
           (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 100));
}

/* ========================================================================== */
/*  Page 1: a layout in percentages                                           */
/* ========================================================================== */

/* Everything below is in percent. Nothing refers to a pixel size. */
static void draw_layout(qg_screen_t *s)
{
    qg_cls(s, QG_BLACK);

    /* Title bar across the top 12%. */
    qg_screen_set_line_width(s, 1);
    qg_box_pct(s, 0, 0, 100, 12, QG_TRANSPARENT, QG_BLUE);

    /* The "die": a circle in the upper middle, a quarter of the smaller
     * dimension in radius, with a thick outline.                            */
    qg_screen_set_line_width(s, 4);
    qg_circle_pct(s, 50, 45, 25, QG_WHITE, QG_DARKGRAY);

    /* A "roll meter" arc hugging the die, running from lower-left over the
     * top to lower-right.
     *
     * CHOOSING THE RIGHT HELPER: qg_arc_pct() measures rx against the width
     * and ry against the height, so on a sideways (landscape) screen it
     * squashes into a flat ellipse. This meter must stay ROUND around a round
     * die, so both radii come from qg_pct_r() (the smaller dimension) and
     * the absolute qg_arc() does the drawing. Mixing the two styles like
     * this is exactly what the qg_pct_*() helpers are for.                 */
    qg_screen_set_line_width(s, 3);
    int16_t meter_r = qg_pct_r(s, 32);
    qg_arc(s, qg_pct_x(s, 50), qg_pct_y(s, 45), meter_r, meter_r, -45, 225, QG_YELLOW);

    /* Three buttons along the bottom. */
    qg_screen_set_line_width(s, 2);
    qg_box_pct(s,  4, 80, 30, 95, QG_LIGHTGREEN, QG_GREEN);
    qg_box_pct(s, 37, 80, 63, 95, QG_LIGHTCYAN,  QG_CYAN);
    qg_box_pct(s, 70, 80, 96, 95, QG_LIGHTRED,   QG_RED);

    /* Corner ticks: lines from each corner 10% inward, to show 0 and 100
     * land exactly on the first and last pixels.                            */
    qg_screen_set_line_width(s, 1);
    qg_line_pct(s,   0,   0,  10,  10, QG_WHITE);
    qg_line_pct(s, 100,   0,  90,  10, QG_WHITE);
    qg_line_pct(s,   0, 100,  10,  90, QG_WHITE);
    qg_line_pct(s, 100, 100,  90,  90, QG_WHITE);

    /* Single pixels at the four "exact" corners: 0 and 100 in both axes. */
    qg_pset_pct(s,   0,   0, QG_YELLOW);
    qg_pset_pct(s, 100,   0, QG_YELLOW);
    qg_pset_pct(s,   0, 100, QG_YELLOW);
    qg_pset_pct(s, 100, 100, QG_YELLOW);
}

static void page_layout(void)
{
    printf("\n--- Page 1: layout in percentages ---\n");
    printf("  Look for: the same layout on both screens, upright and then\n"
           "  turned 90 degrees; corner ticks touching each corner exactly.\n");

    for (int rot = 0; rot <= 1; rot++) {
        printf("  rotation %d:\n", rot ? 90 : 0);
        for (int i = 0; i < 2; i++) {
            qg_screen_set_rotation(screens[i], rot ? QG_ROT_90 : QG_ROT_0);
            uint64_t t0 = time_us_64();
            draw_layout(screens[i]);
            print_ms(names[i], time_us_64() - t0);
        }
        sleep_ms(4000);
    }
    for (int i = 0; i < 2; i++) {
        qg_screen_set_rotation(screens[i], QG_ROT_0);
    }
}

/* ========================================================================== */
/*  Page 2: gauge benchmark                                                   */
/* ========================================================================== */
static void page_gauge(void)
{
    printf("\n--- Page 2: gauge benchmark (no pauses) ---\n");
    printf("  M2 took roughly 12 ms per slice on screen A.\n");

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        const int16_t w = qg_screen_width(s);
        const int16_t r = (int16_t)(w / 2 - 12);
        const int16_t cx = w / 2, cy = (int16_t)(r + 8);

        qg_cls(s, QG_BLACK);
        qg_screen_set_line_width(s, 10);
        qg_arc(s, cx, cy, r, r, -45, 225, QG_DARKGRAY);

        int slices = 0;
        uint64_t t0 = time_us_64();
        for (int deg = 225; deg > -45; deg -= 10) {
            qg_color_t c = (deg > 45) ? QG_LIGHTGREEN : (deg > 0 ? QG_YELLOW : QG_LIGHTRED);
            qg_arc(s, cx, cy, r, r, (int16_t)(deg - 10), (int16_t)deg, c);
            slices++;
        }
        uint64_t us = time_us_64() - t0;

        printf("  %-6s %d slices in %lu.%lu ms -> %lu us per slice\n", names[i],
               slices, (unsigned long)(us / 1000), (unsigned long)((us % 1000) / 100),
               (unsigned long)(us / (uint64_t)slices));
    }
    sleep_ms(3000);
}

/* ========================================================================== */
/*  Page 3: spinning dial                                                     */
/* ========================================================================== */

/*
 * HOW THE DIAL ANIMATES ON AN UNBUFFERED SCREEN
 * The marker moves clockwise by DIAL_STEP_DEG each frame. The old and new
 * positions overlap:
 *
 *      new marker:  [next ........ next+14]
 *      old marker:          [angle ........ angle+14]      (angle = next+6)
 *                                   ^^^^^^^ trailing strip: now uncovered
 *
 * Erasing the WHOLE old marker would blank the overlap for a moment, which
 * the eye sees as flicker. So each frame we only paint the trailing strip
 * back to the track colour, then draw the new marker. Nothing that should
 * stay yellow is ever blanked, and only two small slices are sent per frame.
 * (With a BUF8 framebuffer in M8, you could redraw the whole scene instead.)
 */
#define DIAL_MARKER_DEG 14    /* marker size         */
#define DIAL_STEP_DEG    6    /* movement per frame  */
#define DIAL_SECONDS     4

static void page_dial(void)
{
    printf("\n--- Page 3: spinning dial (%d s) ---\n", DIAL_SECONDS);
    printf("  Look for: a smooth marker spinning clockwise on both screens.\n");

    int16_t cx[2], cy[2], r[2];
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        cx[i] = qg_pct_x(s, 50);
        cy[i] = qg_pct_y(s, 50);
        r[i]  = qg_pct_r(s, 40);

        qg_cls(s, QG_BLACK);
        qg_screen_set_line_width(s, 12);
        qg_arc(s, cx[i], cy[i], r[i], r[i], 0, 0, QG_DARKGRAY);   /* full ring */

        /* Tick marks every 30 degrees, like a combination lock: short, thick
         * arcs just outside the ring. (Arc outlines grow inward, so a width
         * of 7 at radius r+15 covers r+9 to r+15.)                           */
        qg_screen_set_line_width(s, 7);
        for (int t = 0; t < 360; t += 30) {
            qg_arc(s, cx[i], cy[i], (int16_t)(r[i] + 15), (int16_t)(r[i] + 15),
                    (int16_t)(t - 2), (int16_t)(t + 2), QG_WHITE);
        }
        qg_screen_set_line_width(s, 12);
    }

    int angle = 90;            /* start at 12 o'clock, spin clockwise */
    for (int i = 0; i < 2; i++) {
        qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                (int16_t)angle, (int16_t)(angle + DIAL_MARKER_DEG), QG_YELLOW);
    }

    int      frames = 0;
    uint64_t t_end  = time_us_64() + (uint64_t)DIAL_SECONDS * 1000000u;
    uint64_t t0     = time_us_64();

    while (time_us_64() < t_end) {
        int next = angle - DIAL_STEP_DEG;
        for (int i = 0; i < 2; i++) {
            /* Erase only the trailing strip, then draw the new marker. */
            qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                    (int16_t)(next + DIAL_MARKER_DEG), (int16_t)(angle + DIAL_MARKER_DEG),
                    QG_DARKGRAY);
            qg_arc(screens[i], cx[i], cy[i], r[i], r[i],
                    (int16_t)next, (int16_t)(next + DIAL_MARKER_DEG), QG_YELLOW);
        }
        angle = next;
        frames++;
    }
    uint64_t us = time_us_64() - t0;

    unsigned long fps10 = (unsigned long)((uint64_t)frames * 10000000u / us);
    printf("  %d frames (both screens each frame) -> %lu.%lu frames per second\n",
           frames, fps10 / 10, fps10 % 10);

    for (int i = 0; i < 2; i++) {
        qg_screen_set_line_width(screens[i], 1);
    }
    sleep_ms(1500);
}

/* ========================================================================== */
int main(void)
{
    demo_setup("Dice Roller qg4p - Milestone 3");

    while (true) {
        page_layout();
        page_gauge();
        page_dial();
    }
}
