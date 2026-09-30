/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    board.h
 * @brief   The one place the examples learn about your wiring.
 *
 * Every example calls board_init() (or board_init_two()) and then just
 * draws. Change the settings below ONCE to match your hardware, and every
 * example follows. board.c shows the full setup, heavily commented: it's
 * the file to read if you want to see how a screen is brought to life.
 *
 * The defaults match docs/WIRING.md: a 2.0" ST7789 as screen A and, for
 * the two-screen examples, a 3.5" ST7796S (or 2.8" ILI9341) as screen B.
 */
#ifndef BOARD_H
#define BOARD_H

#include "qg4p.h"

/* ========================================================================== */
/*  YOUR SETTINGS: edit these                                                 */
/* ========================================================================== */

/* --- Shared SPI bus (all screens) --- */
#define BOARD_PIN_SCK      18
#define BOARD_PIN_MOSI     19
#define BOARD_PIN_DC       20
#define BOARD_PIN_RST      21        /* or QG_PIN_NONE if not wired */
#define BOARD_SPI_HZ       40000000u /* gives 37.5 MHz; try 20000000u on long wires */

/* --- Screen A: every example uses it --- */
#define BOARD_A_DRIVER     QG_DRIVER_ST7789
#define BOARD_A_WIDTH      240
#define BOARD_A_HEIGHT     320
#define BOARD_A_CS         17
#define BOARD_A_BL         16        /* backlight, or QG_PIN_NONE */
#define BOARD_A_INVERT     true      /* panel settings: see docs/WIRING.md */
#define BOARD_A_BGR        false
#define BOARD_A_MIRROR_X   false

/* --- Screen B: only the two-screen examples use it --- */
#define BOARD_B_DRIVER     QG_DRIVER_ST7796
#define BOARD_B_WIDTH      320
#define BOARD_B_HEIGHT     480
#define BOARD_B_CS         22
#define BOARD_B_BL         15
#define BOARD_B_INVERT     false
#define BOARD_B_BGR        true
#define BOARD_B_MIRROR_X   true
/* For the 2.8" ILI9341 instead: QG_DRIVER_ILI9341, 240 x 320,
 * invert false, bgr true, mirror_x true (the same flags, as it happens). */

/* --- Colour adjustment, per screen ---
 * { red, green, blue gains in percent }, { red, green, blue gammas x 100 }.
 * All 100s = no adjustment. Find your panel's numbers with qg4p_calibrate,
 * which prints a ready-made line to paste here.                           */
#define BOARD_A_ADJUSTED   0         /* 1 = apply the line below (costs 1.3 KB) */
#define BOARD_A_ADJUST     { { 100, 100, 100 }, { 100, 100, 100 } }
#define BOARD_B_ADJUSTED   0
#define BOARD_B_ADJUST     { { 100, 100, 100 }, { 100, 100, 100 } }

/* --- Text scrolling memory, per screen (DIRECT screens only) ---
 * Lines remembered so printing past the bottom can scroll (about 126 bytes
 * each). 0 = none: printing past the bottom clears and starts at the top.  */
#define BOARD_A_TEXT_HISTORY  32
#define BOARD_B_TEXT_HISTORY  0

/* ========================================================================== */
/*  What the examples get                                                     */
/* ========================================================================== */

extern qg_screen_t screen_a;       /* ready after any board_init...()      */
extern qg_screen_t screen_b;       /* ready after board_init_two...()      */

/* The three built-in fonts, already in each screen's slots:
 *   slot 0  font_body   (sans 16)       the default for printing
 *   slot 1  font_title  (sans bold 24)  {f:1} in markup
 *   slot 2  font_small  (mono 12)       {f:2} in markup                   */
extern qg_font_t font_body, font_title, font_small;

/*
 * Bring up the bus and screen A (and, for the _two versions, screen B).
 *
 *   board_init()              screen A, DIRECT
 *   board_init_fb(fb, size)   screen A as a framebuffer (BUF8) screen;
 *                             fb: at least BOARD_A_WIDTH x BOARD_A_HEIGHT bytes
 *   board_init_two()          both screens, DIRECT
 *   board_init_two_fb(fb, n)  both; screen B as a framebuffer screen
 *
 * Why separate functions rather than "pass NULL for no framebuffer"? Only the
 * _fb versions mention the framebuffer backend, and the linker leaves out
 * functions a program never calls. So a program without framebuffers
 * doesn't carry the framebuffer code. Pay for what you use.
 */
void board_init(void);
void board_init_fb(uint8_t *framebuffer, uint32_t size);
void board_init_two(void);
void board_init_two_fb(uint8_t *framebuffer_b, uint32_t size_b);

/** The adjustment settings above (applied only if BOARD_x_ADJUSTED is 1). */
extern const qg_color_adjust_t board_adjust_a, board_adjust_b;

/** A random number from 0 to n - 1 (good enough for games, not for secrets). */
uint32_t board_random(uint32_t n);

#endif /* BOARD_H */
