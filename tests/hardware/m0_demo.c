/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m0_demo.c
 * @brief   Milestone 0 test: bring up the 2.0" ST7789 on the shared bus.
 *
 * WHAT IT CHECKS (watch the screen and the USB serial monitor)
 *
 *   Test 1 - Named colours
 *     Clears the screen to each of the 16 QuickBasic colours in turn and
 *     prints the expected name over USB serial. Use it to tune the panel:
 *       - First colour is WHITE, not BLACK?     -> flip PANEL_INVERT
 *       - RED looks BLUE and BLUE looks RED?    -> flip PANEL_BGR
 *
 *   Test 2 - Orientation and offsets (all four rotations)
 *     Draws a 1-pixel white border (all four edges must be visible; a
 *     missing edge means the panel offsets are wrong) and coloured corner
 *     squares:
 *            RED ........ GREEN
 *             .   (cyan)   .      <- cyan bar marks the TOP edge
 *             .            .
 *            BLUE ....... YELLOW
 *     If the corners are swapped left/right, the panel is mirrored: flip
 *     PANEL_MIRROR_X. If swapped top/bottom, flip PANEL_MIRROR_Y.
 *
 *   Test 3 - Fill speed
 *     Times 32 full-screen clears through the colour cube and reports the
 *     milliseconds per clear and the theoretical frames per second.
 *
 * SERIAL OUTPUT
 *   Open the serial monitor in VSCodium (or `screen /dev/ttyACM0 115200`)
 *   within about 3 seconds of plugging in, or you'll miss the first lines.
 *   The tests loop forever, so you'll see everything on the next pass anyway.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_internal.h"   /* demo-only: raw rectangle fill until M2's qg_box() */

/* ========================================================================== */
/*  Wiring: change these to match your breadboard (see docs/WIRING.md)       */
/* ========================================================================== */
#define PIN_SCK        18   /* SPI0 SCK  -> board SCL / SCK                    */
#define PIN_MOSI       19   /* SPI0 TX   -> board SDA / SDI(MOSI)              */
#define PIN_DC         20   /* shared DC                                       */
#define PIN_RST        21   /* shared reset                                    */
#define PIN_CS_A      17   /* CS of screen A   (2.0" ST7789 in M0)       */
#define PIN_CS_B  22   /* CS of screen B (parked high in M0)     */
#define PIN_BL_A      16   /* backlight, screen A (PURPLE)                   */

/* SPI speed. 40 MHz asks for the fastest "safe" rate; the Pico 2 delivers
 * 37.5 MHz. On long breadboard jumpers, drop to 20000000 if you see
 * speckles or shifted pixels.                                                */
#define SPI_HZ         40000000u

/* Panel settings for the 2.0" GMT020-02 board. Adjust per Test 1 and 2.     */
#define PANEL_INVERT   true
#define PANEL_BGR      false
#define PANEL_MIRROR_X false
#define PANEL_MIRROR_Y false

/* ========================================================================== */

static qg_bus_t    bus;
static qg_screen_t scr_a;

static const char *const rot_names[4] = { "0", "90", "180", "270" };

/* -------------------------------------------------------------------------- */
static void test_named_colors(void)
{
    printf("\n--- Test 1: named colours (1 s each) ---\n");
    for (qg_color_t c = QG_BLACK; c <= QG_WHITE; c++) {
        printf("  %2u  %s\n", (unsigned)c, qg_color_name(c));
        qg_cls(&scr_a, c);
        sleep_ms(1000);
    }
}

/* -------------------------------------------------------------------------- */
static void draw_orientation_pattern(qg_screen_t *s)
{
    const int16_t w  = qg_screen_width(s);
    const int16_t h  = qg_screen_height(s);
    const int16_t sq = 30;   /* corner square size */

    qg_cls(s, QG_BLACK);

    /* 1-pixel border: top, bottom, left, right. */
    qg_int_fill_rect(s, 0,     0,     w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     h - 1, w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     0,     1, h, QG_WHITE);
    qg_int_fill_rect(s, w - 1, 0,     1, h, QG_WHITE);

    /* Corner squares, inset by 2 px so the border stays visible. */
    qg_int_fill_rect(s, 2,          2,          sq, sq, QG_RED);
    qg_int_fill_rect(s, w - sq - 2, 2,          sq, sq, QG_GREEN);
    qg_int_fill_rect(s, 2,          h - sq - 2, sq, sq, QG_BLUE);
    qg_int_fill_rect(s, w - sq - 2, h - sq - 2, sq, sq, QG_YELLOW);

    /* "This way up" marker: a cyan bar centred along the top edge. */
    qg_int_fill_rect(s, w / 2 - 30, 2, 60, 10, QG_CYAN);

    /* Clipping check: this square hangs off the right edge by half. If
     * clipping works, you see only its left half and nothing wraps around
     * to the left side of the screen.                                        */
    qg_int_fill_rect(s, w - 20, h / 2 - 20, 40, 40, QG_LIGHTMAGENTA);
}

static void test_orientation(void)
{
    printf("\n--- Test 2: orientation (3 s each) ---\n");
    printf("  Expect: RED top-left, GREEN top-right, BLUE bottom-left,\n");
    printf("          YELLOW bottom-right, CYAN bar at top, white border on\n");
    printf("          all 4 edges, magenta half-square cut off at right edge.\n");

    for (int r = 0; r < 4; r++) {
        qg_screen_set_rotation(&scr_a, (qg_rotation_t)r);
        printf("  rotation %-3s -> %d x %d  (RAM offset x=%u y=%u)\n",
               rot_names[r],
               qg_screen_width(&scr_a), qg_screen_height(&scr_a),
               (unsigned)scr_a.ram_x_off, (unsigned)scr_a.ram_y_off);
        draw_orientation_pattern(&scr_a);
        sleep_ms(3000);
    }
    qg_screen_set_rotation(&scr_a, QG_ROT_0);
}

/* -------------------------------------------------------------------------- */
static void test_speed(void)
{
    const int frames = 32;

    printf("\n--- Test 3: full-screen fill speed ---\n");

    uint64_t t0 = time_us_64();
    for (int i = 0; i < frames; i++) {
        /* Walk through the 216-colour cube (indices 16..231). */
        qg_cls(&scr_a, (qg_color_t)(16 + (i * 7) % 216));
    }
    uint64_t us = time_us_64() - t0;

    unsigned long us_per = (unsigned long)(us / frames);
    printf("  %d clears in %lu us -> %lu.%02lu ms per clear, ~%lu fps\n",
           frames, (unsigned long)us, us_per / 1000, (us_per % 1000) / 10,
           us_per ? 1000000ul / us_per : 0ul);
}

/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    sleep_ms(3000);   /* give the USB serial monitor time to connect */

    printf("\n=== Dice Roller qg4p - Milestone 0 ===\n");

    /* --- The shared bus ---------------------------------------------------- */
    static const int8_t all_cs[] = { PIN_CS_A, PIN_CS_B };

    const qg_bus_config_t bus_cfg = {
        .spi      = spi0,
        .sck_pin  = PIN_SCK,
        .mosi_pin = PIN_MOSI,
        .dc_pin   = PIN_DC,
        .rst_pin  = PIN_RST,
        .cs_pins  = all_cs,
        .cs_count = (uint8_t)(sizeof(all_cs) / sizeof(all_cs[0])),
    };

    if (qg_bus_init(&bus, &bus_cfg) != QG_OK) {
        printf("Bus init FAILED - check the pin numbers.\n");
        while (true) tight_loop_contents();
    }

    /* --- Screen A: 2.0" ST7789 240x320 -------------------------------- */
    const qg_screen_config_t cfg_a = {
        .driver         = QG_DRIVER_ST7789,
        .cs_pin         = PIN_CS_A,
        .bl_pin         = PIN_BL_A,
        .bl_active_high = true,
        .spi_hz         = SPI_HZ,
        .width          = 240,
        .height         = 320,
        .x_offset       = 0,
        .y_offset       = 0,
        .bgr            = PANEL_BGR,
        .invert         = PANEL_INVERT,
        .mirror_x       = PANEL_MIRROR_X,
        .mirror_y       = PANEL_MIRROR_Y,
        .rotation       = QG_ROT_0,
        .backend        = QG_BACKEND_DIRECT,
    };

    qg_err_t err = qg_screen_init(&scr_a, &bus, &cfg_a);
    if (err != QG_OK) {
        printf("Screen init FAILED (error %d)\n", (int)err);
        while (true) tight_loop_contents();
    }

    printf("Driver: %s, %d x %d, backend %s\n", scr_a.drv->name,
           qg_screen_width(&scr_a), qg_screen_height(&scr_a),
           scr_a.backend->name);
    printf("SPI: requested %lu Hz, actual %lu Hz\n",
           (unsigned long)scr_a.dev.hz_requested,
           (unsigned long)scr_a.dev.hz_actual);

    /* --- Loop the tests forever -------------------------------------------- */
    while (true) {
        test_named_colors();
        test_orientation();
        test_speed();
        sleep_ms(2000);
    }
}
