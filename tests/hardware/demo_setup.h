/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    demo_setup.h
 * @brief   Shared hardware setup for the test programs (M2 onward).
 *
 * Every demo needs the same bus and the same two screens, so that setup lives
 * here once. Change the wiring or board choice in demo_setup.c, and every
 * demo follows.
 */
#ifndef DEMO_SETUP_H
#define DEMO_SETUP_H

#include "qg4p.h"

extern qg_bus_t    bus;
extern qg_screen_t scr_a;       /* 2.0" ST7789                         */
extern qg_screen_t scr_b;   /* 2.8" ILI9341 or 3.5" ST7796S        */

/**
 * Start USB serial, wait for the monitor, bring up the bus and both screens,
 * and print their details. Halts with a message if anything fails.
 */
void demo_setup(const char *title);

/**
 * Like demo_setup(), but makes screen B a framebuffer (BUF8)
 * screen using the buffer you pass in (at least width x height bytes).
 * Pass NULL for an ordinary DIRECT screen B.
 */
void demo_setup_ex(const char *title, uint8_t *fb_b, uint32_t fb_b_size);

#endif /* DEMO_SETUP_H */
