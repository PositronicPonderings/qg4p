/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m1_demo.c
 * @brief   Milestone 1 test: two screens on one shared bus, each with its own
 *          driver, SPI speed and backlight brightness.
 *
 *   screen A      2.0" ST7789           CS GP17, backlight GP16
 *   Screen B  2.8" ILI9341          CS GP22, backlight GP15
 *                  (or the 3.5" ST7796S - set SCREEN_B_BOARD below)
 *
 * WHAT IT CHECKS (watch both screens and the USB serial monitor)
 *
 *   Test 1 - Named colours, both screens together
 *     Both screens should show the SAME colour at the same time. If the
 *     screen B disagrees, tune its B_* panel settings:
 *       - WHITE where BLACK is expected          -> flip B_INVERT
 *       - RED and BLUE swapped                   -> flip B_BGR
 *
 *   Test 2 - Orientation, both screens together
 *     Same pattern as M0: RED top-left, GREEN top-right, BLUE bottom-left,
 *     YELLOW bottom-right, CYAN bar marking the top, full white border.
 *       - Corners swapped left/right             -> flip B_MIRROR_X
 *       - Corners swapped top/bottom             -> flip B_MIRROR_Y
 *     The two screens may turn in opposite directions for 90 and 270; that
 *     is a property of each panel and is fine as long as the pattern itself
 *     is correct.
 *
 *   Test 3 - Brightness
 *     Each backlight fades down and back up while the other stays steady.
 *     If both change together, the two backlight pins are mis-wired.
 *
 *   Test 4 - Bus hand-over
 *     Alternates full-screen clears between the two screens, so the SPI
 *     speed switches on every clear. Both must finish on clean, solid
 *     colours with no stripes or garbage.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_internal.h"   /* demo-only: raw rectangle fill until M2's qg_box() */

/* ========================================================================== */
/*  Which board is screen B?                                        */
/* ========================================================================== */
#define SCREEN_B_ILI9341  1   /* 2.8" red board   */
#define SCREEN_B_ST7796   2   /* 3.5" blue board  */

#define SCREEN_B_BOARD    SCREEN_B_ILI9341

/* ========================================================================== */
/*  Wiring (see docs/WIRING.md)                                              */
/* ========================================================================== */
#define PIN_SCK        18   /* shared   YELLOW */
#define PIN_MOSI       19   /* shared   ORANGE */
#define PIN_DC         20   /* shared   BLUE   */
#define PIN_RST        21   /* shared   WHITE  */
#define PIN_CS_A      17   /* GREEN  */
#define PIN_CS_B  22   /* GREEN  */
#define PIN_BL_A      16   /* PURPLE */
#define PIN_BL_B  15   /* PURPLE */

/* ========================================================================== */
/*  Per-screen settings                                                      */
/* ========================================================================== */

/* screen A: 2.0" ST7789, settings confirmed in M0. */
#define A_SPI_HZ          40000000u   /* -> 37.5 MHz actual */

#if SCREEN_B_BOARD == SCREEN_B_ILI9341
  /* 2.8" ILI9341. Settings taken from the working POC (MADCTL 0x48, no INVON).
   * Confirmed on hardware in M1 at 37.5 MHz.                                */
  #define B_DRIVER    QG_DRIVER_ILI9341
  #define B_W         240
  #define B_H         320
  #define B_SPI_HZ    40000000u   /* -> 37.5 MHz actual */
  #define B_INVERT    false
  #define B_BGR       true
  #define B_MIRROR_X  true
  #define B_MIRROR_Y  false
#else
  /* 3.5" ST7796S. Settings confirmed on hardware in M1.                     */
  #define B_DRIVER    QG_DRIVER_ST7796
  #define B_W         320
  #define B_H         480
  #define B_SPI_HZ    40000000u   /* -> 37.5 MHz actual */
  #define B_INVERT    false
  #define B_BGR       true
  #define B_MIRROR_X  true
  #define B_MIRROR_Y  false
#endif

/* ========================================================================== */

static qg_bus_t    bus;
static qg_screen_t scr_a;
static qg_screen_t scr_b;

static const char *const rot_names[4] = { "0", "90", "180", "270" };

/* -------------------------------------------------------------------------- */
static void halt(const char *msg, int err)
{
    printf("%s (error %d)\n", msg, err);
    while (true) tight_loop_contents();
}

static void print_screen_info(const char *label, const qg_screen_t *s)
{
    printf("%-7s %-8s %3d x %3d  SPI requested %8lu Hz, actual %8lu Hz\n",
           label, s->drv->name, qg_screen_width(s), qg_screen_height(s),
           (unsigned long)s->dev.hz_requested, (unsigned long)s->dev.hz_actual);
}

/* -------------------------------------------------------------------------- */
static void test_named_colors(void)
{
    printf("\n--- Test 1: named colours, both screens (1 s each) ---\n");
    for (qg_color_t c = QG_BLACK; c <= QG_WHITE; c++) {
        printf("  %2u  %s\n", (unsigned)c, qg_color_name(c));
        qg_cls(&scr_a, c);
        qg_cls(&scr_b, c);
        sleep_ms(1000);
    }
}

/* -------------------------------------------------------------------------- */
static void draw_orientation_pattern(qg_screen_t *s)
{
    const int16_t w  = qg_screen_width(s);
    const int16_t h  = qg_screen_height(s);
    const int16_t sq = 30;

    qg_cls(s, QG_BLACK);

    qg_int_fill_rect(s, 0,     0,     w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     h - 1, w, 1, QG_WHITE);
    qg_int_fill_rect(s, 0,     0,     1, h, QG_WHITE);
    qg_int_fill_rect(s, w - 1, 0,     1, h, QG_WHITE);

    qg_int_fill_rect(s, 2,          2,          sq, sq, QG_RED);
    qg_int_fill_rect(s, w - sq - 2, 2,          sq, sq, QG_GREEN);
    qg_int_fill_rect(s, 2,          h - sq - 2, sq, sq, QG_BLUE);
    qg_int_fill_rect(s, w - sq - 2, h - sq - 2, sq, sq, QG_YELLOW);

    qg_int_fill_rect(s, w / 2 - 30, 2, 60, 10, QG_CYAN);
    qg_int_fill_rect(s, w - 20, h / 2 - 20, 40, 40, QG_LIGHTMAGENTA);
}

static void test_orientation(void)
{
    printf("\n--- Test 2: orientation, both screens (3 s each) ---\n");
    for (int r = 0; r < 4; r++) {
        qg_screen_set_rotation(&scr_a, (qg_rotation_t)r);
        qg_screen_set_rotation(&scr_b, (qg_rotation_t)r);
        printf("  rotation %-3s -> screen A %d x %d, screen B %d x %d\n", rot_names[r],
               qg_screen_width(&scr_a), qg_screen_height(&scr_a),
               qg_screen_width(&scr_b), qg_screen_height(&scr_b));
        draw_orientation_pattern(&scr_a);
        draw_orientation_pattern(&scr_b);
        sleep_ms(3000);
    }
    qg_screen_set_rotation(&scr_a, QG_ROT_0);
    qg_screen_set_rotation(&scr_b, QG_ROT_0);
}

/* -------------------------------------------------------------------------- */
static void fade(qg_screen_t *s)
{
    for (int p = 100; p >= 0; p -= 2) { qg_screen_set_brightness(s, (uint8_t)p); sleep_ms(20); }
    for (int p = 0;   p <= 100; p += 2) { qg_screen_set_brightness(s, (uint8_t)p); sleep_ms(20); }
}

static void test_brightness(void)
{
    printf("\n--- Test 3: brightness ---\n");
    qg_cls(&scr_a, QG_WHITE);
    qg_cls(&scr_b, QG_WHITE);

    printf("  screen A fades, screen B steady\n");
    fade(&scr_a);
    sleep_ms(500);

    printf("  Screen B fades, screen A steady\n");
    fade(&scr_b);
    sleep_ms(500);

    printf("  Fixed levels: screen A 100%%, 50%%, 10%%; screen B stays at 100%%\n");
    const uint8_t levels[] = { 100, 50, 10 };
    for (unsigned i = 0; i < sizeof(levels); i++) {
        qg_screen_set_brightness(&scr_a, levels[i]);
        sleep_ms(1000);
    }

    printf("  Off / on keeps the dimmed level (screen A should return at 10%%)\n");
    qg_screen_backlight(&scr_a, false);
    sleep_ms(1000);
    qg_screen_backlight(&scr_a, true);
    sleep_ms(1500);

    qg_screen_set_brightness(&scr_a, 100);
}

/* -------------------------------------------------------------------------- */
static void test_bus_handover(void)
{
    const int rounds = 16;

    printf("\n--- Test 4: bus hand-over (speed switches every clear) ---\n");

    uint64_t t0 = time_us_64();
    for (int i = 0; i < rounds; i++) {
        qg_cls(&scr_a,     (qg_color_t)(16 + (i * 11) % 216));
        qg_cls(&scr_b, (qg_color_t)(16 + (i * 13) % 216));
    }
    uint64_t us = time_us_64() - t0;

    /* Finish on known colours so the result is easy to judge by eye. */
    qg_cls(&scr_a, QG_GREEN);
    qg_cls(&scr_b, QG_BLUE);

    printf("  %d rounds (%d clears) in %lu us, %lu us per round\n",
           rounds, rounds * 2, (unsigned long)us, (unsigned long)(us / rounds));
    printf("  Expect: screen A solid GREEN, screen B solid BLUE, no stripes.\n");
    sleep_ms(3000);
}

/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    sleep_ms(3000);   /* time to open the serial monitor */

    printf("\n=== Dice Roller qg4p - Milestone 1 ===\n");

    /* --- Shared bus --------------------------------------------------------- */
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
    qg_err_t err = qg_bus_init(&bus, &bus_cfg);
    if (err != QG_OK) halt("Bus init FAILED", err);

    /* --- screen A ---------------------------------------------------------- */
    const qg_screen_config_t cfg_a = {
        .driver = QG_DRIVER_ST7789, .cs_pin = PIN_CS_A,
        .bl_pin = PIN_BL_A, .bl_active_high = true, .spi_hz = A_SPI_HZ,
        .width = 240, .height = 320, .x_offset = 0, .y_offset = 0,
        .bgr = false, .invert = true, .mirror_x = false, .mirror_y = false,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
    };
    err = qg_screen_init(&scr_a, &bus, &cfg_a);
    if (err != QG_OK) halt("screen A init FAILED", err);

    /* --- Screen B ------------------------------------------------------ */
    const qg_screen_config_t cfg_b = {
        .driver = B_DRIVER, .cs_pin = PIN_CS_B,
        .bl_pin = PIN_BL_B, .bl_active_high = true, .spi_hz = B_SPI_HZ,
        .width = B_W, .height = B_H, .x_offset = 0, .y_offset = 0,
        .bgr = B_BGR, .invert = B_INVERT,
        .mirror_x = B_MIRROR_X, .mirror_y = B_MIRROR_Y,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
    };
    err = qg_screen_init(&scr_b, &bus, &cfg_b);
    if (err != QG_OK) halt("Screen B init FAILED", err);

    print_screen_info("A", &scr_a);
    print_screen_info("B", &scr_b);

    /* --- Loop the tests forever --------------------------------------------- */
    while (true) {
        test_named_colors();
        test_orientation();
        test_brightness();
        test_bus_handover();
    }
}
