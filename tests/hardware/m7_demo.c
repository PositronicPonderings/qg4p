/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    m7_demo.c
 * @brief   Milestone 7 test: loading images and text from the asset pack.
 *
 * Built by tests/hardware/CMakeLists.txt as its own target (qg4p_m7.uf2).
 *
 * THE PACK (separately from the firmware):
 *     python3 tools/mkpack.py tests/hardware/pack --out build/assets
 *     then hold BOOTSEL, plug in, and drag build/assets.uf2 onto the drive.
 *
 * Unlike M6, no images are compiled into this program: everything on screen
 * comes from the pack. Run it once BEFORE loading the pack to see the
 * "no pack" message.
 *
 * PAGES
 *   1  Contents   the pack's file list (screen A) and a text file from it (screen B)
 *   2  Images     every image in the pack, drawn straight from flash
 *   3  Errors     what a missing file looks like, handled gracefully
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "qg4p.h"
#include "qg_asset.h"
#include "demo_setup.h"

static qg_font_t f_body  = QG_FONT_INIT(qg_font_sans_16,      QG_DEFAULT, 1);
static qg_font_t f_title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW,  1);
static qg_font_t f_mono  = QG_FONT_INIT(qg_font_mono_12,      QG_DEFAULT, 1);

static qg_screen_t *const screens[2] = { &scr_a, &scr_b };

extern char __flash_binary_end;   /* end of this firmware in flash (pico-sdk) */

/* -------------------------------------------------------------------------- */
/*  Helper: open an image from the pack in one call                           */
/* -------------------------------------------------------------------------- */
/*
 * This small bridge is all it takes to join the two modules: the pack says
 * where the bytes are and whether they're transparent; qg4p draws them.
 */
static bool open_asset_image(const char *name, qg_image_t *img)
{
    qg_asset_t a;
    qg_asset_err_t err = qg_asset_find(name, &a);
    if (err != QG_ASSET_OK) {
        printf("  %s: %s\n", name, qg_asset_err_str(err));
        return false;
    }
    uint8_t flags = (a.flags & QG_ASSET_FLAG_TRANSPARENT) ? QG_IMAGE_TRANSPARENT : 0;
    return qg_image_open(img, a.data, a.size, flags) == QG_OK;
}

/* -------------------------------------------------------------------------- */
static void show_no_pack(qg_asset_err_t err)
{
    char buf[160];
    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        qg_cls(s, QG_BLACK);
        qg_locate(s, 6, 6);
        qg_println(s, "{f:1}{c:LIGHTRED}No asset pack");
        snprintf(buf, sizeof buf, "{c:YELLOW}%s", qg_asset_err_str(err));
        qg_println(s, buf);
        qg_println(s, "");
        if (err == QG_ASSET_ERR_OVERLAP) {
            qg_println(s, "The firmware is bigger than 1 MB. Move the pack "
                           "(QG_ASSET_PACK_OFFSET and mkpack.py --offset).");
        } else {
            qg_println(s, "On the PC:");
            qg_println(s, "{f:2}python3 tools/mkpack.py tests/hardware/pack --out build/assets");
            qg_println(s, "");
            qg_println(s, "Then hold BOOTSEL, plug in, and drag {c:LIGHTGREEN}assets.uf2{c:} onto the drive.");
        }
    }
}

/* ========================================================================== */
/*  Page 1: contents                                                          */
/* ========================================================================== */
static void page_contents(void)
{
    static const char *const type_names[] = { "other", "image", "sound", "text" };
    char buf[80];
    qg_asset_t a;

    /* screen A: the file list. */
    qg_screen_t *s = &scr_a;
    qg_cls(s, QG_BLACK);
    qg_locate(s, 4, 4);
    qg_println(s, "{f:1}Asset pack");
    snprintf(buf, sizeof buf, "{f:2}%u files, %lu bytes", qg_asset_count(),
             (unsigned long)qg_asset_pack_size());
    qg_println(s, buf);
    qg_println(s, "");
    for (uint16_t i = 0; i < qg_asset_count(); i++) {
        qg_asset_get(i, &a);
        snprintf(buf, sizeof buf, "{f:2}{c:WHITE}%s", a.name);
        qg_println(s, buf);
        snprintf(buf, sizeof buf, "{f:2}{c:DARKGRAY}  %lu B, %s%s", (unsigned long)a.size,
                 a.type < 4 ? type_names[a.type] : "?",
                 (a.flags & QG_ASSET_FLAG_TRANSPARENT) ? ", transparent" : "");
        qg_println(s, buf);
    }

    /* Screen B: a text file, printed straight from flash. The pack doesn't add
     * a terminating NUL, so copy it into a buffer first (or print it with a
     * length-limited loop).                                                 */
    s = &scr_b;
    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 6);
    qg_println(s, "{f:1}From text/welcome.txt:");
    if (qg_asset_find("text/welcome.txt", &a) == QG_ASSET_OK) {
        char text[200];
        uint32_t n = a.size < sizeof(text) - 1 ? a.size : sizeof(text) - 1;
        for (uint32_t i = 0; i < n; i++) text[i] = (char)a.data[i];
        text[n] = '\0';
        qg_println(s, text);
    }
}

/* ========================================================================== */
/*  Page 2: images                                                            */
/* ========================================================================== */
static void page_images(void)
{
    qg_image_t banner, landscape, d20, potion, d20_prebuilt;
    bool ok = open_asset_image("art/banner.bmp",      &banner)
            & open_asset_image("art/landscape.bmp",   &landscape)
            & open_asset_image("dice/d20.bmp",        &d20)
            & open_asset_image("items/potion.bmp",    &potion)
            & open_asset_image("ui/d20_prebuilt.bmp", &d20_prebuilt);
    if (!ok) return;

    for (int i = 0; i < 2; i++) {
        qg_screen_t *s = screens[i];
        const int16_t w = qg_screen_width(s);
        qg_cls(s, QG_BLACK);
        qg_image_draw_scaled(s, &banner, 0, 0, w, 48);
        qg_image_draw_fit(s, &landscape, 0, 50, w, 160, QG_ALIGN_CENTER);
        qg_image_draw(s, &d20, 8, 220);
        qg_image_draw(s, &d20_prebuilt, 80, 220);
        qg_image_draw_scaled(s, &potion, 156, 220, 64, 64);
    }
}

/* ========================================================================== */
/*  Page 3: errors                                                            */
/* ========================================================================== */
static void page_errors(void)
{
    qg_screen_t *s = &scr_a;
    qg_asset_t a;
    char buf[96];

    qg_cls(s, QG_BLACK);
    qg_locate(s, 6, 6);
    qg_println(s, "{f:1}Missing files");
    const char *names[] = { "dice/d21.bmp", "Dice/d20.bmp", "dice/d20.bmp" };
    for (int i = 0; i < 3; i++) {
        qg_asset_err_t err = qg_asset_find(names[i], &a);
        snprintf(buf, sizeof buf, "%s\n  {c:%s}%s", names[i],
                 err == QG_ASSET_OK ? "LIGHTGREEN" : "LIGHTRED", qg_asset_err_str(err));
        qg_println(s, buf);
    }
    qg_println(s, "{c:DARKGRAY}(names are case-sensitive)");

    qg_cls(&scr_b, QG_BLACK);
}

/* ========================================================================== */
int main(void)
{
    demo_setup("Dice Roller qg4p - Milestone 7");
    for (int i = 0; i < 2; i++) {
        qg_screen_set_font(screens[i], 0, &f_body);
        qg_screen_set_font(screens[i], 1, &f_title);
        qg_screen_set_font(screens[i], 2, &f_mono);
    }

    /* Where things are in flash. */
    uintptr_t fw_end = (uintptr_t)&__flash_binary_end;
    printf("\nFirmware: 0x10000000 - 0x%08lx (%lu KB)\n", (unsigned long)fw_end,
           (unsigned long)((fw_end - 0x10000000u) / 1024));
    printf("Pack area starts at 0x%08lx\n", (unsigned long)(0x10000000u + QG_ASSET_PACK_OFFSET));

    qg_asset_err_t err = qg_asset_init();
    printf("Asset pack: %s\n", qg_asset_err_str(err));
    if (err != QG_ASSET_OK) {
        show_no_pack(err);
        while (true) tight_loop_contents();   /* load the pack; the Pico restarts */
    }

    uint64_t t0 = time_us_64();
    err = qg_asset_verify();
    printf("Checksum: %s (%lu bytes checked in %lu us)\n", qg_asset_err_str(err),
           (unsigned long)qg_asset_pack_size(), (unsigned long)(time_us_64() - t0));
    if (err != QG_ASSET_OK) {
        show_no_pack(err);
        while (true) tight_loop_contents();
    }

    qg_asset_t a;
    t0 = time_us_64();
    for (int i = 0; i < 1000; i++) qg_asset_find("items/potion.bmp", &a);
    printf("qg_asset_find: %lu ns per lookup\n", (unsigned long)(time_us_64() - t0));

    while (true) {
        printf("\n--- Page 1: contents ---\n");
        page_contents();
        sleep_ms(5000);

        printf("\n--- Page 2: images from the pack ---\n");
        page_images();
        sleep_ms(5000);

        printf("\n--- Page 3: missing files ---\n");
        page_errors();
        sleep_ms(4000);
    }
}
