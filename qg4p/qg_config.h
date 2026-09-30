/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_config.h
 * @brief   Compile-time configuration for the QG4P.
 *
 * LAYER:   Configuration (read by every other layer)
 * DEPENDS: nothing
 *
 * Everything in this file is a "knob" you may want to turn without touching
 * library code. Each value is wrapped in #ifndef so you can also override it
 * from CMake, e.g.:
 *
 *     target_compile_definitions(qg4p PUBLIC QG_TEXT_CELL_PIXELS=0)
 *
 */
#ifndef QG_CONFIG_H
#define QG_CONFIG_H

/**
 * SPI clock used by the bus before any screen has asked for its own speed.
 * Deliberately slow and safe; each screen switches to its own speed when it
 * takes the bus (see qg_hal_begin()).
 */
#ifndef QG_BUS_BOOT_HZ
#define QG_BUS_BOOT_HZ 1000000u
#endif

/**
 * Backlight PWM frequency in Hz.
 *
 * Brightness is set by switching the backlight on and off very quickly
 * (PWM, "pulse-width modulation"); the eye averages it into a steady level.
 * 10 kHz is far too fast to see as flicker, and slow enough for the small
 * switching transistor on each display board to follow cleanly.
 */
#ifndef QG_BL_PWM_HZ
#define QG_BL_PWM_HZ 10000u
#endif

/**
 * Font slots per screen. Slot 0 is the screen's default font; the others
 * are selected with {f:n} markup. Each slot is just a pointer.
 */
#ifndef QG_MAX_FONTS
#define QG_MAX_FONTS 4
#endif

/**
 * Largest character cell (in pixels) that opaque text draws in one piece.
 * Opaque text builds each character cell, background and all, in a small
 * RAM buffer and sends it as one block. The buffer costs 2 bytes per pixel:
 * 2048 pixels = 4 KB, enough for a 24 px font at scale 2. Bigger cells still
 * work, using a slower two-step method. The buffer is linked into any program
 * that prints; set this to 0 to remove it if you never use opaque text (or
 * don't mind it drawing the slower way).
 */
#ifndef QG_TEXT_CELL_PIXELS
#define QG_TEXT_CELL_PIXELS 2048
#endif

/**
 * Text scrolling on DIRECT screens: to scroll, the screen is cleared and the
 * remembered lines reprinted one row higher. The memory for those lines is
 * YOURS to provide, per screen, in the screen's config:
 *
 *     static qg_text_line_t history_a[QG_TEXT_HISTORY_LINES];
 *     cfg.text_history       = history_a;
 *     cfg.text_history_lines = QG_TEXT_HISTORY_LINES;
 *
 * QG_TEXT_HISTORY_LINES is just a sensible size: enough lines to fill a
 * 480-pixel-tall screen in the smallest font. Each line costs
 * QG_TEXT_HISTORY_CHARS + 6 bytes (126 by default), so 32 lines is about
 * 4 KB. Without history, printing past the bottom clears the screen and
 * carries on from the top. Framebuffer screens never need it: they scroll
 * by moving pixels.
 */
#ifndef QG_TEXT_HISTORY_LINES
#define QG_TEXT_HISTORY_LINES 32
#endif
#ifndef QG_TEXT_HISTORY_CHARS
#define QG_TEXT_HISTORY_CHARS 120
#endif

/** Default tab width in pixels (change per screen with qg_screen_set_tab_width). */
#ifndef QG_TAB_WIDTH
#define QG_TAB_WIDTH 40
#endif

/**
 * Images: the widest image (and widest drawn row) the decoder handles, in
 * pixels. Sizes three small row buffers: 480 x 5 bytes = 2.4 KB.
 */
#ifndef QG_IMAGE_MAX_WIDTH
#define QG_IMAGE_MAX_WIDTH 480
#endif

/**
 * Images: pixels sent to the screen per transfer. Several rows are gathered
 * into one block before sending, which cuts bus overhead. 2 bytes per pixel:
 * 2048 pixels = 4 KB.
 */
#ifndef QG_IMAGE_BLOCK_PIXELS
#define QG_IMAGE_BLOCK_PIXELS 2048
#endif

/**
 * Framebuffer (BUF8) screens: pixels converted and sent per DMA transfer
 * during qg_screen_flush(). Two buffers of this size are used in turn, so
 * one is converted while the other is sent: 2 x 1024 x 2 bytes = 4 KB.
 */
#ifndef QG_BUF8_CHUNK_PIXELS
#define QG_BUF8_CHUNK_PIXELS 1024
#endif

/**
 * qg_paint() working space: pending spans to fill, 4 bytes each. 1024 is
 * enough for any ordinary shape; truly maze-like regions may need more.
 * If it runs out, the fill stops early and returns QG_ERR_OVERFLOW.
 */
#ifndef QG_PAINT_STACK
#define QG_PAINT_STACK 1024
#endif

#endif /* QG_CONFIG_H */
