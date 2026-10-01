# Too busy to read a manual

Everything, on one page. Every function exists exactly as written here; details are one click away in the [reference](reference/README.md). Include `qg4p.h` (and `qg_asset.h` for the asset pack).

**Types, so the lines below stay short:** `scr` is a `qg_screen_t *` (`&my_screen`). `x`, `y`, `w`, `h`, `r` are pixels (`int16_t`); in `_pct` functions they're percentages, 0 to 100 (`uint8_t`). Colours are `qg_color_t`. `→ err` means it returns `qg_err_t` (`QG_OK` on success).

## Set up

```c example=quickref_setup compile-only
static qg_bus_t    bus;
static qg_screen_t scr_a;
static qg_font_t   body = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);

static const int8_t cs[] = { 17 };                    /* EVERY screen's CS on this bus */
const qg_bus_config_t bus_cfg = { .spi = spi0, .sck_pin = 18, .mosi_pin = 19,
    .dc_pin = 20, .rst_pin = 21, .cs_pins = cs, .cs_count = 1 };
qg_bus_init(&bus, &bus_cfg);

const qg_screen_config_t cfg = { .driver = QG_DRIVER_ST7789, .cs_pin = 17,
    .bl_pin = 16, .bl_active_high = true, .spi_hz = 40000000u,
    .width = 240, .height = 320, .invert = true };
qg_screen_init(&scr_a, &bus, &cfg);
qg_screen_set_font(&scr_a, 0, &body);                 /* no font, no text */
```

Drivers: `QG_DRIVER_ST7789` `QG_DRIVER_ILI9341` `QG_DRIVER_ST7796`. Framebuffer screen: add `.backend = QG_BACKEND_BUF8, .framebuffer = fb, .framebuffer_size = sizeof fb`. Scrolling text on a DIRECT screen: add `.text_history = hist, .text_history_lines = QG_TEXT_HISTORY_LINES` (`static qg_text_line_t hist[QG_TEXT_HISTORY_LINES];`). Panel looks wrong? Flip `.invert`, `.bgr`, `.mirror_x`, `.mirror_y`. SCK/MOSI pins: [the rule](07-getting-started.md#choosing-pins).

## Screens

```c
qg_bus_init(bus, cfg)                         → err   /* the shared wires */
qg_screen_init(scr, bus, cfg)                 → err   /* chip on, cleared, lit */
qg_screen_width(scr)   qg_screen_height(scr)          /* change with rotation */
qg_screen_set_rotation(scr, QG_ROT_0 | _90 | _180 | _270) → err
qg_screen_backlight(scr, on)
qg_screen_set_brightness(scr, percent)   qg_screen_get_brightness(scr)
qg_screen_flush(scr)                          /* framebuffer: show changes (no-op on DIRECT) */
qg_screen_flush_all(scr)                      /* framebuffer: resend everything */
qg_screen_is_buffered(scr)                    /* a framebuffer screen? */
```

## Drawing

```c
qg_cls(scr, color)                            /* CLS (the view, if one is set) */
qg_pset(scr, x, y, color)                     /* PSET */
qg_preset(scr, x, y, color)                   /* PRESET: QG_DEFAULT = background */
qg_line(scr, x1, y1, x2, y2, color)           /* LINE (x1,y1)-(x2,y2) */
qg_box(scr, x1, y1, x2, y2, stroke, fill)     /* LINE ...,B  /  ,BF */
qg_circle(scr, x, y, r, stroke, fill)         /* CIRCLE */
qg_ellipse(scr, x, y, rx, ry, stroke, fill)   /* CIRCLE with aspect */
qg_arc(scr, x, y, rx, ry, start_deg, end_deg, color)   /* 0 = 3 o'clock, counter-clockwise */
qg_screen_set_line_width(scr, width)   qg_screen_get_line_width(scr)
qg_screen_set_line_style(scr, 0xF0F0)         /* dashes; 0xFFFF = solid again */
qg_screen_set_colors(scr, fg, bg)             /* COLOR fg, bg */
qg_point(scr, x, y)                    → color  /* POINT; framebuffer only */
qg_paint(scr, x, y, fill, border)      → err    /* PAINT; framebuffer only */
qg_view(scr, x1, y1, x2, y2, move_origin)     /* VIEW; move_origin: (0,0) = its corner */
qg_view_reset(scr)                            /* VIEW (off) */
qg_view_width(scr)   qg_view_height(scr)
```

`stroke` or `fill` = `QG_TRANSPARENT` leaves that part out. Thick outlines grow inward.

## Percentages

```c
qg_pset_pct(scr, x, y, color)
qg_line_pct(scr, x1, y1, x2, y2, color)
qg_box_pct(scr, x1, y1, x2, y2, stroke, fill)
qg_circle_pct(scr, x, y, r, stroke, fill)             /* r: % of the smaller side */
qg_ellipse_pct(scr, x, y, rx, ry, stroke, fill)       /* rx: % of width, ry: of height */
qg_arc_pct(scr, x, y, rx, ry, start_deg, end_deg, color)
qg_view_pct(scr, x1, y1, x2, y2, move_origin)
qg_pct_x(scr, pct)   qg_pct_y(scr, pct)   qg_pct_r(scr, pct)   /* % -> pixels */
```

## GET and PUT (sprites)

```c
static uint8_t blk[QG_BLOCK_BYTES(w, h)];
qg_get(scr, x1, y1, x2, y2, blk, sizeof blk)   → err  /* GET; framebuffer only */
qg_put(scr, x, y, blk, mode)                   → err  /* PUT */
qg_block_width(blk)   qg_block_height(blk)
```

Modes: `QG_PUT_PSET` `QG_PUT_PRESET` `QG_PUT_TRANSPARENT` (skips 255), and, framebuffer only, `QG_PUT_AND` `QG_PUT_OR` `QG_PUT_XOR` (XOR twice = erased).

## Text

```c
static qg_font_t f = QG_FONT_INIT(qg_font_sans_16, color, scale);   /* scale 1..4 */
qg_font_create(&font_data, color, scale)     → qg_font_t
qg_font_check(&font_data)                    → err
qg_font_line_height(font)
qg_screen_set_font(scr, slot, font)          → err   /* slots 0..3; 0 = default */
qg_locate(scr, x, y)   qg_locate_pct(scr, x, y)       /* LOCATE (pixels) */
qg_print(scr, "text")                                 /* PRINT "text"; */
qg_println(scr, "text")                               /* PRINT "text"  */
qg_print_at(scr, x, y, "text", color, font)           /* font NULL = slot 0 */
qg_print_align(scr, y, "text", QG_ALIGN_LEFT | _CENTER | _RIGHT)
qg_print_box(scr, x, y, width, "text", align)         /* wrapped in a column */
qg_text_measure(scr, "text", font, max_width, &w, &h)
qg_pos(scr)   qg_csrlin(scr)                          /* POS(0), CSRLIN (pixels) */
qg_screen_set_text_bg(scr, color)                     /* QG_TRANSPARENT = off */
qg_screen_set_tab_width(scr, pixels)
qg_screen_set_wrap(scr, on)   qg_screen_set_scroll(scr, on)
```

Fonts: `qg_font_mono_12` `qg_font_sans_16` `qg_font_sans_bold_24`. Numbers: `snprintf` first, then print.

**Markup:** `\n` `\t` · `{c:RED}` `{c:200}` colour · `{f:1}` font slot · `{s:2}` scale · `{x:120}` column · `{c:}` `{f:}` `{s:}` back to default · `{{` a literal `{`

## Images

```c
qg_image_t img;
qg_image_open(&img, data, size, QG_IMAGE_TRANSPARENT or 0)   → err
qg_image_width(&img)   qg_image_height(&img)
qg_image_draw(scr, &img, x, y)
qg_image_draw_scaled(scr, &img, x, y, w, h)
qg_image_draw_fit(scr, &img, x, y, w, h, align)             /* keeps its shape */
qg_palette_load_image(scr, &img, 16)          → count       /* framebuffer: exact colours */
```

Make images: `python3 tools/img2bmp8.py in.png --out out.bmp [--c-array img_name] [--no-dither]`.

## Colour

```c
qg_color_from_rgb(r, g, b, scr->palette)       → color   /* nearest; once, not per frame */
qg_color_from_name("red", 3)                   → 0..15, or -1
qg_color_name(color)                           → "RED", or NULL
qg_palette_set(scr, index, r, g, b)            → err     /* framebuffer: recolours at flush */
qg_palette_get(scr->palette, index, &r, &g, &b)
qg_palette_reset(scr)
qg_palette_standard()   qg_palette_copy_standard(dst)
qg_screen_set_color_adjust(scr, &adj, &state)  → err     /* panel calibration; NULL = off */
QG_RGB565(r, g, b)
```

Named: `QG_BLACK` `QG_BLUE` `QG_GREEN` `QG_CYAN` `QG_RED` `QG_MAGENTA` `QG_BROWN` `QG_LIGHTGRAY` `QG_DARKGRAY` `QG_LIGHTBLUE` `QG_LIGHTGREEN` `QG_LIGHTCYAN` `QG_LIGHTRED` `QG_LIGHTMAGENTA` `QG_YELLOW` `QG_WHITE` (0 to 15). Special: `QG_TRANSPARENT` (don't draw), `QG_DEFAULT` (the screen's colour), `QG_NONE` (no colour).

## Asset pack

```c
qg_asset_init()                        → asset err   /* once, at start-up */
qg_asset_t a;
qg_asset_find("icons/star.bmp", &a)    → asset err   /* a.data, a.size, a.flags */
qg_asset_count()   qg_asset_get(i, &a)   qg_asset_pack_size()
qg_asset_verify()                      → asset err   /* every byte; slow */
qg_asset_err_str(err)                  → text
qg_asset_init_at(address)              → asset err
```

Make a pack: `python3 tools/mkpack.py folder --out build/assets`, then drag `build/assets.uf2` onto the Pico. Link `qg4p_assets`.

## Flash (photosensitivity-safer)

```c
#include "qg_flash.h"                          /* not in qg4p.h; linked only if used */
static qg_flash_t fl;                          /* yours: static (zeroed), one per screen */
qg_flash_start(&fl, scr, NULL, now_ms)         → err   /* NULL: 3 flashes, 1 a second, to 50 % */
qg_flash_config_t c = { .period_ms, .count, .depth, .color };   /* 0 = default; all clamped */
qg_flash_update(&fl, now_ms)                   → running?   /* every loop; never blocks */
qg_flash_cancel(&fl, now_ms, at_once)          /* false = ease back smoothly */
qg_flash_active(&fl)   qg_flash_is_saturated_red(scr, color)
```

Dims the backlight smoothly and back; no pixel touched, DIRECT or framebuffer. Whatever you ask for: at most 2 flashes a second, 5 per start, no step over `QG_FLASH_MAX_STEP`. A saturated-red foreground, background or `.color` gets `QG_ERR_ARG`. *Safer*, not safe: offer a setting that turns flashing off. [Details](reference/flash.md).

## The eight things that bite

1. **Text needs a font in slot 0.** No font, no text, no error.
2. **Line width and style stay set** until you change them back (`1`, `0xFFFF`).
3. **Changing numbers: opaque text in a monospaced font**, or bits of the old number show.
4. **`qg_point`, `qg_paint`, `qg_get` and bitwise `qg_put` need a framebuffer screen.**
5. **Framebuffer screens show nothing until `qg_screen_flush`.**
6. **PAINT escapes through a one-pixel gap** and floods everything.
7. **Reset the view** (`qg_view_reset`) when you're done with it.
8. **Every `.c` file must be in `add_executable`**, and the program's name must match everywhere in `CMakeLists.txt`.

Stuck? [Troubleshooting](06-troubleshooting.md). Wiring? [Getting started](07-getting-started.md).
