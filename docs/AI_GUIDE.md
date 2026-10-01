# QG4P — AI working guide

Condensed, exact reference for AI assistants writing code with **QG4P** (QuickGraphics 4 Pico) for the **Dice Roller** project. Library v1.0.0. Repo: https://github.com/PositronicPonderings/qg4p (MIT-0). Human docs: `docs/manual/` in the repo (one-page summary: `docs/manual/quick-reference.md`). Everything below is verified against the v1.0.0 source.

## 1. What it is
C11 graphics library, Raspberry Pi Pico 2 (RP2350), Pico SDK, SPI TFT screens (ST7789, ILI9341, ST7796S). QuickBasic-style API (`qg_cls`, `qg_line`, `qg_circle`, `qg_paint`, `qg_locate`, `qg_print`...), fonts with inline markup, 8-bit BMP images, flash asset pack, optional framebuffer (BUF8) screens. Several screens share one SPI bus. Single-core; not thread-safe (shared static work buffers).

**Design rule — "if you don't use it, you don't pay for it":** optional code is linked only when named (`QG_BACKEND_BUF8`, `QG_DRIVER_*` are references to the objects themselves); optional memory (framebuffers, text history, colour-adjust tables) is declared by the caller. Preserve this in any new code: never reference optional features "just in case". Check with `python3 tools/size_audit.py build`.

## 2. This project's hardware (confirmed on hardware)
Pico 2, breadboard, all screens on **spi0**, 37.5 MHz (`spi_hz = 40000000u`), no MISO.

| Signal | GP | Phys pin |
|---|---|---|
| SCK (shared) | 18 | 24 |
| MOSI (shared) | 19 | 25 |
| DC (shared) | 20 | 26 |
| RST (shared) | 21 | 27 |
| Screen A (DM) CS / backlight | 17 / 16 | 22 / 21 |
| Screen B (player) CS / backlight | 22 / 15 | 29 / 20 |

| Screen | Board | Driver | Size | invert | bgr | mirror_x | Notes |
|---|---|---|---|---|---|---|---|
| A (DM) | 2.0" GMT020-02 | `QG_DRIVER_ST7789` | 240x320 | true | false | false | IPS-like, good colour |
| B (player) | 3.5" | `QG_DRIVER_ST7796` (ST7796S) | 320x480 | false | true | true | TN panel: narrow viewing angle, greys lean blue; candidate for `qg_screen_set_color_adjust` (calibrate with `examples/calibrate.c`, `CALIBRATE_B 1`) |
| B alt | 2.8" red ILI9341 | `QG_DRIVER_ILI9341` | 240x320 | false | true | true | earlier player screen |

All `mirror_y = false`. Backlights active-high; GP15 and GP16 are on different PWM slices (independent dimming). GP26–28 kept free (ADC, e.g. battery). Final product panels not chosen yet (target ~3–4", ~$10, prefer IPS). Wire colours: SCK yellow, MOSI orange, CS green, DC blue, RST white, BL purple, VCC red, GND black.

## 3. Repo layout and build
```
qg4p/            library (CMake targets: qg4p; qg4p_assets for the asset pack)
  qg4p.h         the one header (+ assets/qg_asset.h for assets)
  qg_config.h    compile-time settings
examples/        16 examples, each its own target qg4p_<name>; wiring in examples/board.h
tests/hardware/  milestone test programs qg4p_m0..m8, qg4p_new_commands
tests/host/      PC tests: sh tests/host/run_tests.sh  (17 checks, gcc + python3 + Pillow + numpy)
tools/           ttf2qg.py, img2bmp8.py, mkpack.py, size_report.py, size_audit.py
docs/manual/     the manual
```
Using it in a project:
```cmake
add_subdirectory(qg4p)
add_executable(my_app main.c)                       # list EVERY .c file
target_link_libraries(my_app pico_stdlib qg4p)      # + qg4p_assets if using the pack
pico_enable_stdio_usb(my_app 1)
pico_add_extra_outputs(my_app)
# settings: target_compile_definitions(qg4p PUBLIC QG_TEXT_CELL_PIXELS=0)
```
Load without BOOTSEL once a USB-stdio program is running: `picotool load -f -x build/.../app.uf2`.

## 4. Minimal setup (this project's two screens)
```c
#include "pico/stdlib.h"
#include "qg4p.h"

static qg_bus_t bus;
static qg_screen_t dm, player;
static qg_text_line_t dm_history[QG_TEXT_HISTORY_LINES];   /* only if dm scrolls text */
static uint8_t player_fb[320 * 480];                        /* only if player is BUF8 */
static qg_font_t body  = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);
static qg_font_t title = QG_FONT_INIT(qg_font_sans_bold_24, QG_YELLOW, 1);
static qg_font_t small = QG_FONT_INIT(qg_font_mono_12, QG_DEFAULT, 1);

void screens_init(void)
{
    static const int8_t cs[] = { 17, 22 };                 /* EVERY CS on the bus */
    const qg_bus_config_t bc = { .spi = spi0, .sck_pin = 18, .mosi_pin = 19,
        .dc_pin = 20, .rst_pin = 21, .cs_pins = cs, .cs_count = 2 };
    qg_bus_init(&bus, &bc);

    const qg_screen_config_t a = { .driver = QG_DRIVER_ST7789, .cs_pin = 17, .bl_pin = 16,
        .bl_active_high = true, .spi_hz = 40000000u, .width = 240, .height = 320,
        .invert = true, .text_history = dm_history, .text_history_lines = QG_TEXT_HISTORY_LINES };
    qg_screen_init(&dm, &bus, &a);

    const qg_screen_config_t b = { .driver = QG_DRIVER_ST7796, .cs_pin = 22, .bl_pin = 15,
        .bl_active_high = true, .spi_hz = 40000000u, .width = 320, .height = 480,
        .bgr = true, .mirror_x = true,
        .backend = QG_BACKEND_BUF8, .framebuffer = player_fb, .framebuffer_size = sizeof player_fb };
    qg_screen_init(&player, &bus, &b);

    qg_screen_t *s[2] = { &dm, &player };
    for (int i = 0; i < 2; i++) {                           /* slot 0 REQUIRED for text */
        qg_screen_set_font(s[i], 0, &body);
        qg_screen_set_font(s[i], 1, &title);                /* {f:1} */
        qg_screen_set_font(s[i], 2, &small);                /* {f:2}: monospaced */
    }
}
```
Check every `qg_err_t` in real code (`QG_OK` = 0). `qg_bus_t`, `qg_screen_t`, fonts, framebuffers, history and adjust state must outlive their use (static/global).

## 5. Rules that the signatures don't tell you
**Coordinates/colour**
- Pixels are `int16_t`, (0,0) top-left; everything clips to the screen (or view); off-screen values are fine.
- Colours are palette indices (`qg_color_t`, 16-bit). `QG_TRANSPARENT` (255) = don't draw this part; `QG_DEFAULT` = screen fg (bg for `qg_cls`/`qg_preset`); `QG_NONE` = no colour (from `qg_point`).
- Shapes take `stroke, fill`; pass `QG_TRANSPARENT` for the part you don't want. Box corners inclusive, any order.
- Thick outlines (box/circle/ellipse/arc) grow **inward**; thick lines are centred.
- `qg_screen_set_line_width` and `qg_screen_set_line_style` are **sticky** — reset to `1` / `0xFFFF` after use. Style: 16-bit, MSB first, 1 = draw; 0 also means solid; applies to lines and box outlines only.
- `qg_arc` angles in degrees, 0 = 3 o'clock, counter-clockwise; `start == end` = full outline.
- Standard palette: 0–15 QuickBasic colours (RED is 170,0,0; YELLOW is 255,255,85), 16–231 6x6x6 cube, 232–254 greys, 255 reserved. `qg_color_from_rgb(r,g,b, palette or NULL)` searches 255 entries — call once, cache the result.

**Percentages (`*_pct`)**: `uint8_t` 0..100; 0 = first pixel, 100 = last, rounded down. Circle r = % of the smaller side; ellipse/arc rx = % of width, ry = % of height. With a view that moved the origin, % measures the view. `qg_view_pct` corners always measure the screen.

**View (`qg_view`)**: corners in screen pixels, inclusive. `move_origin=false` clips only; `true` makes (0,0) the view's corner and percentages measure it. While set: `qg_cls` clears only the view, text wraps at its right edge, printing doesn't scroll. Always `qg_view_reset`. Rotation resets it.

**Text**
- A screen starts with **no fonts**; without slot 0 nothing prints (no error).
- `x, y` = top-left of the line. UTF-8. Colour precedence: argument (if not `QG_DEFAULT`) > font's colour > screen fg; `{c:}` markup overrides.
- Markup: `\n` `\r` `\t` · `{c:RED}`/`{c:200}` · `{f:0..3}` · `{s:1..4}` · `{x:120}` column · `{c:}` `{f:}` `{s:}` reset · `{{` literal brace.
- `qg_print`/`qg_println` use the cursor (`qg_locate` sets cursor and left margin) and wrap/scroll; `qg_print_at`, `qg_print_align`, `qg_print_box` never scroll.
- Scrolling: BUF8 moves pixels (all content). DIRECT reprints remembered text lines — needs `.text_history` (else clears and restarts at top); shapes are cleared.
- Changing numbers: opaque text (`qg_screen_set_text_bg(s, QG_BLACK)`) **and** a monospaced font (`{f:2}`/`qg_font_mono_12`) with fixed-width formatting (`%5d`); proportional fonts leave remnants.
- Format numbers with `snprintf` first; there is no numeric print.

**Framebuffer (BUF8) screens**
- Drawing goes to RAM; **nothing shows until `qg_screen_flush(s)`**, which sends only the changed rectangle. `qg_screen_flush` is a no-op on DIRECT, so it's safe everywhere.
- Only BUF8: `qg_point`, `qg_paint`, `qg_get`, `qg_put` AND/OR/XOR. On DIRECT they return `QG_NONE` / `QG_ERR_UNSUPPORTED`.
- `qg_palette_set` on BUF8 recolours existing pixels at next flush (palette animation; marks whole screen dirty).
- Images on BUF8 are matched to the screen palette; `qg_palette_load_image(s, &img, 16)` gives exact colours (load first, then draw; overwrites entries 16+).
- Full-screen flush 320x480 ≈ 72 ms (SPI-limited) → ~14 full frames/s; small changes are proportionally faster.

**Images**: 8-bit BMP (plain or RLE8), ≤ 480 px wide, read in place from flash. `QG_IMAGE_TRANSPARENT` makes index 255 see-through. Scaling is nearest-neighbour. Make with `tools/img2bmp8.py in.png --out out.bmp [--c-array img_name] [--no-dither]`.

**GET/PUT**: block = 4-byte header + w*h indices; size `QG_BLOCK_BYTES(w,h)`. GET must be fully inside screen/view. PUT clips. Modes `QG_PUT_PSET`, `_PRESET` (255-n), `_TRANSPARENT` (skip 255), `_AND`, `_OR`, `_XOR` (XOR twice restores; use background 0 for XOR sprites).

**PAINT**: `border` colour stops the fill; `QG_DEFAULT` = bucket fill of the start colour. 4-connected; a 1-px gap leaks. Returns `QG_ERR_OVERFLOW` if the work list (`QG_PAINT_STACK` 1024) runs out.

**Asset pack**: built by `tools/mkpack.py folder --out build/assets` (PNG/JPG/GIF→.bmp, names keep folders with `/`, ≤35 chars), loaded at flash offset 1 MB (`QG_ASSET_PACK_OFFSET`). `qg_asset_init()` once; `qg_asset_find(name,&a)` (~1 µs) gives `a.data`, `a.size`, `a.type`, `a.flags` (`QG_ASSET_FLAG_TRANSPARENT` → open image with `QG_IMAGE_TRANSPARENT`). Files are not NUL-terminated. Handle a missing pack gracefully.

**Colour adjustment** (per-panel calibration): `qg_screen_set_color_adjust(s, &adj, &state)`; `adj = { .gain = {r,g,b percent 0..100}, .gamma = {r,g,b x100, 50..300} }`; `state` is a caller-owned `qg_color_adjust_state_t` (1,290 B) kept while active; `NULL` adj = off. Applied only to what's sent; no per-pixel cost. DIRECT: redraw after setting.

## 6. API (all public functions, exact)
### Screens
```c
qg_err_t qg_screen_init(qg_screen_t *s, qg_bus_t *bus, const qg_screen_config_t *cfg);
qg_err_t qg_screen_set_rotation(qg_screen_t *s, qg_rotation_t rot);
void qg_screen_backlight(qg_screen_t *s, bool on);
void qg_screen_set_brightness(qg_screen_t *s, uint8_t percent);
uint8_t qg_screen_get_brightness(const qg_screen_t *s);
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg);
void qg_screen_set_line_width(qg_screen_t *s, uint8_t width);
uint8_t qg_screen_get_line_width(const qg_screen_t *s);
void qg_screen_flush(qg_screen_t *s);
void qg_screen_flush_all(qg_screen_t *s);
bool qg_screen_is_buffered(const qg_screen_t *s);
int16_t qg_screen_width(const qg_screen_t *s);
int16_t qg_screen_height(const qg_screen_t *s);
```
### Bus
```c
qg_err_t qg_bus_init(qg_bus_t *bus, const qg_bus_config_t *cfg);
```
### Drawing
```c
void qg_view(qg_screen_t *s, int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool move_origin);
void qg_view_reset(qg_screen_t *s);
int16_t qg_view_width(const qg_screen_t *s);
int16_t qg_view_height(const qg_screen_t *s);
void qg_cls(qg_screen_t *s, qg_color_t color);
void qg_pset(qg_screen_t *s, int16_t x, int16_t y, qg_color_t color);
void qg_preset(qg_screen_t *s, int16_t x, int16_t y, qg_color_t color);
void qg_screen_set_line_style(qg_screen_t *s, uint16_t pattern);
void qg_line(qg_screen_t *s, int16_t x1, int16_t y1, int16_t x2, int16_t y2, qg_color_t color);
void qg_box(qg_screen_t *s, int16_t x1, int16_t y1, int16_t x2, int16_t y2, qg_color_t stroke, qg_color_t fill);
void qg_circle(qg_screen_t *s, int16_t cx, int16_t cy, int16_t r, qg_color_t stroke, qg_color_t fill);
void qg_ellipse(qg_screen_t *s, int16_t cx, int16_t cy, int16_t rx, int16_t ry, qg_color_t stroke, qg_color_t fill);
void qg_arc(qg_screen_t *s, int16_t cx, int16_t cy, int16_t rx, int16_t ry, int16_t start_deg, int16_t end_deg, qg_color_t color);
qg_color_t qg_point(qg_screen_t *s, int16_t x, int16_t y);
qg_err_t qg_paint(qg_screen_t *s, int16_t x, int16_t y, qg_color_t fill, qg_color_t border);
```
### Percentages
```c
int16_t qg_pct_x(const qg_screen_t *s, uint8_t pct);
int16_t qg_pct_y(const qg_screen_t *s, uint8_t pct);
int16_t qg_pct_r(const qg_screen_t *s, uint8_t pct);
void qg_view_pct(qg_screen_t *s, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool move_origin);
void qg_pset_pct(qg_screen_t *s, uint8_t x, uint8_t y, qg_color_t color);
void qg_line_pct(qg_screen_t *s, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, qg_color_t color);
void qg_box_pct(qg_screen_t *s, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, qg_color_t stroke, qg_color_t fill);
void qg_circle_pct(qg_screen_t *s, uint8_t cx, uint8_t cy, uint8_t r, qg_color_t stroke, qg_color_t fill);
void qg_ellipse_pct(qg_screen_t *s, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry, qg_color_t stroke, qg_color_t fill);
void qg_arc_pct(qg_screen_t *s, uint8_t cx, uint8_t cy, uint8_t rx, uint8_t ry, int16_t start_deg, int16_t end_deg, qg_color_t color);
```
### GET/PUT
```c
qg_err_t qg_get(qg_screen_t *s, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t *buf, uint32_t buf_size);
qg_err_t qg_put(qg_screen_t *s, int16_t x, int16_t y, const uint8_t *buf, qg_put_t mode);
int16_t qg_block_width(const uint8_t *buf);
int16_t qg_block_height(const uint8_t *buf);
```
### Text
```c
qg_font_t qg_font_create(const lv_font_t *data, qg_color_t color, uint8_t scale);
qg_err_t qg_font_check(const lv_font_t *data);
int16_t qg_font_line_height(const qg_font_t *font);
qg_err_t qg_screen_set_font(qg_screen_t *s, uint8_t slot, const qg_font_t *font);
void qg_screen_set_text_bg(qg_screen_t *s, qg_color_t bg);
void qg_screen_set_tab_width(qg_screen_t *s, int16_t pixels);
void qg_screen_set_wrap(qg_screen_t *s, bool on);
void qg_screen_set_scroll(qg_screen_t *s, bool on);
int16_t qg_pos(const qg_screen_t *s);
int16_t qg_csrlin(const qg_screen_t *s);
void qg_locate(qg_screen_t *s, int16_t x, int16_t y);
void qg_locate_pct(qg_screen_t *s, uint8_t x, uint8_t y);
void qg_print(qg_screen_t *s, const char *text);
void qg_println(qg_screen_t *s, const char *text);
void qg_print_at(qg_screen_t *s, int16_t x, int16_t y, const char *text, qg_color_t color, const qg_font_t *font);
void qg_print_align(qg_screen_t *s, int16_t y, const char *text, qg_align_t align);
void qg_print_box(qg_screen_t *s, int16_t x, int16_t y, int16_t width, const char *text, qg_align_t align);
void qg_text_measure(qg_screen_t *s, const char *text, const qg_font_t *font, int16_t max_width, int16_t *w, int16_t *h);
```
### Images
```c
qg_err_t qg_image_open(qg_image_t *img, const uint8_t *data, uint32_t size, uint8_t flags);
int16_t qg_image_width(const qg_image_t *img);
int16_t qg_image_height(const qg_image_t *img);
void qg_image_draw(qg_screen_t *s, const qg_image_t *img, int16_t x, int16_t y);
void qg_image_draw_scaled(qg_screen_t *s, const qg_image_t *img, int16_t x, int16_t y, int16_t w, int16_t h);
void qg_image_draw_fit(qg_screen_t *s, const qg_image_t *img, int16_t x, int16_t y, int16_t w, int16_t h, qg_align_t align);
int qg_palette_load_image(qg_screen_t *s, const qg_image_t *img, uint8_t first);
```
### Colour
```c
void qg_palette_copy_standard(uint16_t dst[256]);
const uint16_t *qg_palette_standard(void);
const char *qg_color_name(qg_color_t c);
void qg_palette_get(const uint16_t *palette, qg_color_t index, uint8_t *r, uint8_t *g, uint8_t *b);
qg_color_t qg_color_from_rgb(uint8_t r, uint8_t g, uint8_t b, const uint16_t *palette);
qg_err_t qg_palette_set(qg_screen_t *s, qg_color_t index, uint8_t r, uint8_t g, uint8_t b);
void qg_palette_reset(qg_screen_t *s);
qg_err_t qg_screen_set_color_adjust(qg_screen_t *s, const qg_color_adjust_t *adj, qg_color_adjust_state_t *state);
int qg_color_from_name(const char *name, size_t len);
```
### Assets (qg_asset.h, link qg4p_assets)
```c
qg_asset_err_t qg_asset_init(void);
qg_asset_err_t qg_asset_init_at(const uint8_t *pack);
qg_asset_err_t qg_asset_find(const char *name, qg_asset_t *out);
uint16_t qg_asset_count(void);
qg_asset_err_t qg_asset_get(uint16_t i, qg_asset_t *out);
uint32_t qg_asset_pack_size(void);
qg_asset_err_t qg_asset_verify(void);
const char *qg_asset_err_str(qg_asset_err_t err);
```

Hardware layer (`qg_hal_*`, `qg_driver_*`) is for driver authors only.

## 7. Types and constants
- Errors `qg_err_t`: `QG_OK` 0, `QG_ERR_ARG` -1, `QG_ERR_UNSUPPORTED` -2, `QG_ERR_OVERFLOW` -4. Assets `qg_asset_err_t`: `QG_ASSET_OK`, `_ERR_NO_PACK`, `_ERR_VERSION`, `_ERR_CORRUPT`, `_ERR_NOT_FOUND`, `_ERR_OVERLAP`, `_ERR_NOT_READY`.
- Colours: `QG_BLACK` `QG_BLUE` `QG_GREEN` `QG_CYAN` `QG_RED` `QG_MAGENTA` `QG_BROWN` `QG_LIGHTGRAY` `QG_DARKGRAY` `QG_LIGHTBLUE` `QG_LIGHTGREEN` `QG_LIGHTCYAN` `QG_LIGHTRED` `QG_LIGHTMAGENTA` `QG_YELLOW` `QG_WHITE` (0–15); `QG_TRANSPARENT`, `QG_DEFAULT`, `QG_NONE`; `QG_RGB565(r,g,b)`.
- `QG_ROT_0/90/180/270` (direction panel-dependent); `QG_ALIGN_LEFT/CENTER/RIGHT`; `QG_PUT_*`; `QG_IMAGE_TRANSPARENT`; `QG_PIN_NONE`; `QG_BACKEND_DIRECT` (default/NULL), `QG_BACKEND_BUF8`; `QG_DRIVER_ST7789/ILI9341/ST7796`; `QG_FONT_INIT(data,color,scale)`; `QG_BLOCK_BYTES(w,h)`; `QG_COLOR_ADJUST_NONE`; `QG_TEXT_HISTORY_LINES` (32, suggested size).
- Fonts: `qg_font_mono_12`, `qg_font_sans_16`, `qg_font_sans_bold_24` (`lv_font_t`, LVGL fmt_txt 1 bpp). New fonts: `tools/ttf2qg.py X.ttf --size N --name my_font --out my_font.c`.
- `qg_bus_config_t`: `spi, sck_pin, mosi_pin, dc_pin, rst_pin, cs_pins, cs_count`. SCK/MOSI rule: GPIO n is SPI (n/8)%2; SCK if n%4==2, MOSI if n%4==3 (spi0 SCK 2/6/18/22, MOSI 3/7/19; spi1 SCK 10/14/26, MOSI 11/15/27); `qg_bus_init` rejects mismatches.
- `qg_screen_config_t`: `driver, cs_pin, bl_pin, bl_active_high, spi_hz, width, height, x_offset, y_offset, invert, bgr, mirror_x, mirror_y, rotation, backend, framebuffer, framebuffer_size, text_history, text_history_lines`.
- `qg_asset_t`: `name, data, size, type, flags`.
- Settings (`qg_config.h`, override on target `qg4p` PUBLIC): `QG_TEXT_CELL_PIXELS` 2048 (0 removes opaque-text fast path), `QG_PAINT_STACK` 1024, `QG_IMAGE_MAX_WIDTH` 480, `QG_IMAGE_BLOCK_PIXELS` 2048, `QG_BUF8_CHUNK_PIXELS` 1024, `QG_TAB_WIDTH` 40, `QG_MAX_FONTS` 4, `QG_TEXT_HISTORY_CHARS` 120, `QG_BUS_BOOT_HZ` 1 MHz, `QG_BL_PWM_HZ` 10 kHz.

## 8. Memory and speed (measured, Pico 2, 37.5 MHz)
- RAM: `qg_screen_t` 684 B; text history 126 B/line; framebuffer w*h B (76.8 KB at 240x320, 153.6 KB at 320x480); colour-adjust state 1,290 B. Typical: two DIRECT screens + text + images ≈ 17 KB; + 320x480 framebuffer ≈ 172 KB (of 520 KB). Library code ≈ 20 KB flash + fonts 1.5–3.5 KB each.
- Speed: clear 240x320 36 ms; full flush 320x480 72 ms; opaque number update 1.2 ms; 240x160 image 26 ms; arc slice 0.74 ms; `qg_paint` region 1–4 ms; `qg_asset_find` ~1 µs. Mostly SPI-bound (Release ≈ Debug).

## 9. Gotchas (most frequent first)
1. No font in slot 0 → no text, no error.
2. Line width/style stay set.
3. BUF8: nothing appears without `qg_screen_flush`.
4. DIRECT animation: erase by redrawing background only in the old area (use `qg_view` as a clip patch), then draw; erasing by drawing in bg colour also erases what it crossed.
5. Each new animation sequence must clear leftovers of the previous one (e.g. redraw the table before a new dice roll).
6. Numbers: opaque + monospaced + fixed width.
7. PAINT leaks through 1-px gaps; paints over non-border colours (text).
8. GET fully on-screen; save-under sprites must stay on-screen.
9. Black screen: wiring by GP number (grounds between pins shift counting), SCK/MOSI pin rule, driver choice, try `spi_hz = 1000000u`, CS of every screen listed on the bus.
10. Wrong colours: invert (negative image), bgr (red↔blue, green fine), mirror_x (backwards text), offsets (edge garbage). Run `examples/colour_check.c`.
11. CMake: target name must match everywhere; every .c listed; re-configure after adding files/targets.

## 10. Conventions for new code in this project
- C11, Pico SDK, heavily commented "teaching" style: explain why, not just what.
- New source files start with:
  ```c
  /* SPDX-License-Identifier: MIT-0 */
  /* SPDX-AI-Disclosure: ai-generated */
  /* SPDX-AI-Model: claude-opus-5-5 */
  /* SPDX-AI-Provider: Anthropic */
  ```
  (Python/shell/CMake: same lines with `#`.) The repo test `check_disclosure.py` enforces this; record the actual model used.
- No personal credits in code or docs; provenance notes for third-party material are fine.
- Keep "pay for what you use": optional memory caller-provided, optional code reached only via names the caller writes.
- Library changes: run `sh tests/host/run_tests.sh` (golden images must stay identical unless the change is intended), update `CHANGELOG.md`, and keep `docs/manual/quick-reference.md` complete (`check_quickref.py`).
- The examples' `board.h` / `board.c` show the wiring-in-one-place pattern (`board_init()`, `board_init_fb()`, `board_init_two()`, `board_init_two_fb()`).
