/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    board.c
 * @brief   Screen setup for the examples: everything hello.c doesn't show.
 *
 * Reading order, if you're here to learn how a screen comes to life:
 *   1. the bus: shared wires, every CS pin parked high first
 *   2. each screen: which chip, which pins, the panel's quirks
 *   3. fonts in slots, so {f:1} and {f:2} work in every example
 *   4. colour adjustment, if board.h asks for it
 * Memory the library needs from us (text history, colour tables) is
 * declared here only when board.h asks for it.
 *
 * Settings live in board.h. This file just uses them.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

qg_screen_t screen_a, screen_b;
qg_font_t   font_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
qg_font_t   font_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
qg_font_t   font_small = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

const qg_color_adjust_t board_adjust_a = BOARD_A_ADJUST;
const qg_color_adjust_t board_adjust_b = BOARD_B_ADJUST;

/* Memory the library needs from us, declared only if board.h asks for it. */
#if BOARD_A_ADJUSTED
static qg_color_adjust_state_t adjust_state_a;          /* 1,280 bytes */
#endif
#if BOARD_B_ADJUSTED
static qg_color_adjust_state_t adjust_state_b;
#endif
#if BOARD_A_TEXT_HISTORY > 0
static qg_text_line_t history_a[BOARD_A_TEXT_HISTORY];  /* ~126 bytes a line */
#endif
#if BOARD_B_TEXT_HISTORY > 0
static qg_text_line_t history_b[BOARD_B_TEXT_HISTORY];
#endif

static qg_bus_t bus;
static uint32_t rng_state;

/* If a screen won't start, there's no screen to complain on, so blink the
 * Pico's own LED instead, forever, and say why over USB serial.            */
static void fail(const char *what, qg_err_t err)
{
    printf("board: %s failed (error %d). Check the wiring and board.h.\n", what, (int)err);
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    while (true) { gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN); sleep_ms(200); }
#else
    while (true) tight_loop_contents();
#endif
}

static void start_bus(void)
{
    stdio_init_all();                      /* printf() over USB, for messages */

    /* 1. THE BUS. List every screen's CS pin, even ones not used by this
     *    example: the library holds them all high, so an idle screen can
     *    never mistake traffic for its own.                               */
    static const int8_t cs_pins[] = { BOARD_A_CS, BOARD_B_CS };
    const qg_bus_config_t cfg = {
        .spi = spi0, .sck_pin = BOARD_PIN_SCK, .mosi_pin = BOARD_PIN_MOSI,
        .dc_pin = BOARD_PIN_DC, .rst_pin = BOARD_PIN_RST,
        .cs_pins = cs_pins, .cs_count = 2,
    };
    qg_err_t err = qg_bus_init(&bus, &cfg);
    if (err != QG_OK) fail("the SPI bus", err);

    rng_state = (uint32_t)time_us_64() | 1u;     /* seed the dice */
}

/* 4. COLOUR. A panel whose colours are a little off can be corrected, with
 *    numbers from board.h (see the calibrate example).                    */
static void adjust_screens(bool two)
{
#if BOARD_A_ADJUSTED
    qg_screen_set_color_adjust(&screen_a, &board_adjust_a, &adjust_state_a);
#endif
#if BOARD_B_ADJUSTED
    if (two) qg_screen_set_color_adjust(&screen_b, &board_adjust_b, &adjust_state_b);
#endif
    (void)two;
}

static void add_fonts(qg_screen_t *s)
{
    /* 3. FONTS. The screen keeps pointers to these, so they're globals.   */
    qg_screen_set_font(s, 0, &font_body);
    qg_screen_set_font(s, 1, &font_title);
    qg_screen_set_font(s, 2, &font_small);
}

static void start_a(const struct qg_backend *backend, uint8_t *fb, uint32_t size)
{
    /* 2. A SCREEN: chip, pins, and the panel's quirks (docs/WIRING.md
     *    explains how to find invert/bgr/mirror for a new board).         */
    const qg_screen_config_t cfg = {
        .driver = BOARD_A_DRIVER, .cs_pin = BOARD_A_CS,
        .bl_pin = BOARD_A_BL, .bl_active_high = true, .spi_hz = BOARD_SPI_HZ,
        .width = BOARD_A_WIDTH, .height = BOARD_A_HEIGHT,
        .invert = BOARD_A_INVERT, .bgr = BOARD_A_BGR, .mirror_x = BOARD_A_MIRROR_X,
        .rotation = QG_ROT_0,
        .backend = backend, .framebuffer = fb, .framebuffer_size = size,
#if BOARD_A_TEXT_HISTORY > 0
        .text_history = history_a, .text_history_lines = BOARD_A_TEXT_HISTORY,
#endif
    };
    qg_err_t err = qg_screen_init(&screen_a, &bus, &cfg);
    if (err != QG_OK) fail("screen A", err);
    add_fonts(&screen_a);
}

static void start_b(const struct qg_backend *backend, uint8_t *fb, uint32_t size)
{
    const qg_screen_config_t cfg = {
        .driver = BOARD_B_DRIVER, .cs_pin = BOARD_B_CS,
        .bl_pin = BOARD_B_BL, .bl_active_high = true, .spi_hz = BOARD_SPI_HZ,
        .width = BOARD_B_WIDTH, .height = BOARD_B_HEIGHT,
        .invert = BOARD_B_INVERT, .bgr = BOARD_B_BGR, .mirror_x = BOARD_B_MIRROR_X,
        .rotation = QG_ROT_0,
        .backend = backend, .framebuffer = fb, .framebuffer_size = size,
#if BOARD_B_TEXT_HISTORY > 0
        .text_history = history_b, .text_history_lines = BOARD_B_TEXT_HISTORY,
#endif
    };
    qg_err_t err = qg_screen_init(&screen_b, &bus, &cfg);
    if (err != QG_OK) fail("screen B", err);
    add_fonts(&screen_b);
}

/* Only the _fb versions mention QG_BACKEND_BUF8, so only programs that call
 * them carry the framebuffer code (see board.h).                          */
void board_init(void)
{
    start_bus();
    start_a(QG_BACKEND_DIRECT, NULL, 0);
    adjust_screens(false);
}

void board_init_fb(uint8_t *framebuffer, uint32_t size)
{
    start_bus();
    start_a(QG_BACKEND_BUF8, framebuffer, size);
    adjust_screens(false);
}

void board_init_two(void)
{
    start_bus();
    start_a(QG_BACKEND_DIRECT, NULL, 0);
    start_b(QG_BACKEND_DIRECT, NULL, 0);
    adjust_screens(true);
}

void board_init_two_fb(uint8_t *framebuffer_b, uint32_t size_b)
{
    start_bus();
    start_a(QG_BACKEND_DIRECT, NULL, 0);
    start_b(QG_BACKEND_BUF8, framebuffer_b, size_b);
    adjust_screens(true);
}

uint32_t board_random(uint32_t n)
{
    /* xorshift32: three shifts and three XORs, and the numbers look random
     * enough for dice. (For anything that matters, use the Pico SDK's
     * pico_rand, which draws on real hardware noise.)                      */
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return n ? rng_state % n : 0;
}
