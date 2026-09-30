/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    demo_setup.c
 * @brief   Shared hardware setup for the test programs (M2 onward).
 *
 * All panel settings here were confirmed on hardware in M1
 * (see docs/WIRING.md, section 4).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "demo_setup.h"

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

qg_bus_t    bus;
qg_screen_t scr_a;
qg_screen_t scr_b;

static void halt(const char *msg, int err)
{
    printf("%s (error %d)\n", msg, err);
    while (true) tight_loop_contents();
}

static void print_screen_info(const char *label, const qg_screen_t *s)
{
    printf("%-7s %-8s %3d x %3d  SPI %8lu Hz  %s\n", label, s->drv->name,
           qg_screen_width(s), qg_screen_height(s),
           (unsigned long)s->dev.hz_actual, s->backend->name);
}

/*
 * The shared setup, told which backend screen B uses. Only demo_setup_ex()
 * below ever passes QG_BACKEND_BUF8, and the linker leaves out functions a
 * program never calls, so tests that use demo_setup() don't carry the
 * framebuffer code. (It used to be the other way round, demo_setup() calling
 * demo_setup_ex(), which quietly put 5 KB of framebuffer into every test.)
 */
static void setup(const char *title, const struct qg_backend *backend_b,
                  uint8_t *fb_b, uint32_t fb_b_size)
{
    stdio_init_all();
    sleep_ms(3000);   /* time to open the serial monitor */
    printf("\n=== %s ===\n", title);

    /* --- Shared bus --------------------------------------------------------- */
    static const int8_t all_cs[] = { PIN_CS_A, PIN_CS_B };
    const qg_bus_config_t bus_cfg = {
        .spi = spi0, .sck_pin = PIN_SCK, .mosi_pin = PIN_MOSI,
        .dc_pin = PIN_DC, .rst_pin = PIN_RST,
        .cs_pins = all_cs, .cs_count = (uint8_t)(sizeof(all_cs) / sizeof(all_cs[0])),
    };
    qg_err_t err = qg_bus_init(&bus, &bus_cfg);
    if (err != QG_OK) halt("Bus init FAILED", err);

    /* --- screen A: 2.0" ST7789 -------------------------------------------- */
    /* Text scrolling memory: the M5 and M8 tests scroll on both screens.
     * (A framebuffer screen B scrolls by moving pixels and doesn't use it.) */
    static qg_text_line_t history_a[QG_TEXT_HISTORY_LINES], history_b[QG_TEXT_HISTORY_LINES];

    const qg_screen_config_t cfg_a = {
        .driver = QG_DRIVER_ST7789, .cs_pin = PIN_CS_A,
        .bl_pin = PIN_BL_A, .bl_active_high = true, .spi_hz = 40000000u,
        .width = 240, .height = 320,
        .bgr = false, .invert = true, .mirror_x = false, .mirror_y = false,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
        .text_history = history_a, .text_history_lines = QG_TEXT_HISTORY_LINES,
    };
    err = qg_screen_init(&scr_a, &bus, &cfg_a);
    if (err != QG_OK) halt("screen A init FAILED", err);

    /* --- Screen B ------------------------------------------------------ */
#if SCREEN_B_BOARD == SCREEN_B_ILI9341
    qg_screen_config_t cfg_b = {
        .driver = QG_DRIVER_ILI9341, .cs_pin = PIN_CS_B,
        .bl_pin = PIN_BL_B, .bl_active_high = true, .spi_hz = 40000000u,
        .width = 240, .height = 320,
        .bgr = true, .invert = false, .mirror_x = true, .mirror_y = false,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
        .text_history = history_b, .text_history_lines = QG_TEXT_HISTORY_LINES,
    };
#else
    qg_screen_config_t cfg_b = {
        .driver = QG_DRIVER_ST7796, .cs_pin = PIN_CS_B,
        .bl_pin = PIN_BL_B, .bl_active_high = true, .spi_hz = 40000000u,
        .width = 320, .height = 480,
        .bgr = true, .invert = false, .mirror_x = true, .mirror_y = false,
        .rotation = QG_ROT_0, .backend = QG_BACKEND_DIRECT,
        .text_history = history_b, .text_history_lines = QG_TEXT_HISTORY_LINES,
    };
#endif
    if (fb_b != NULL) {
        cfg_b.backend            = backend_b;
        cfg_b.framebuffer        = fb_b;
        cfg_b.framebuffer_size   = fb_b_size;
        cfg_b.text_history       = NULL;          /* not needed: see above */
        cfg_b.text_history_lines = 0;
    }
    err = qg_screen_init(&scr_b, &bus, &cfg_b);
    if (err != QG_OK) halt("Screen B init FAILED", err);

    print_screen_info("A", &scr_a);
    print_screen_info("B", &scr_b);
}

void demo_setup(const char *title)
{
    setup(title, QG_BACKEND_DIRECT, NULL, 0);
}

void demo_setup_ex(const char *title, uint8_t *fb_b, uint32_t fb_b_size)
{
    setup(title, fb_b ? QG_BACKEND_BUF8 : QG_BACKEND_DIRECT, fb_b, fb_b_size);
}
