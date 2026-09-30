# QG4P: Design (rev 3, as built)

> **Superseded by the manual's [Part 9, How it works](manual/09-how-it-works.md)**, which covers the same ground and is kept up to date. This is the design record from development, kept for history.

> Written as the design record of the Dice Roller's graphics library, before the rename to QG4P. Names are updated to `qg_*`; "DM" and "player" screens are now screens A and B.

Rev 1 and rev 2 of this document were the plan. Rev 3 records the library as it was actually built through milestone 8, including every decision that changed along the way. Hardware results and fixes are in `MILESTONES.md`; memory figures are in `SIZES.md`.

## 1. Goals and constraints

The goals, all met:

- **Several screens on one SPI bus,** each with its own controller chip, speed, panel settings and backlight.
- **A QuickBasic-flavoured drawing API:** points, lines, boxes, circles, ellipses, arcs, flood fill.
- **Text:** compact fonts, inline markup, word wrap, scrolling, measurement.
- **Images:** independent X/Y scaling or locked aspect ratio, plus transparency.
- **Smooth animation available** for future puzzles, per screen, without costing screens that don't need it.
- **Heavily commented,** so the code works as a teaching tool.

The constraints, and how they came out:

| Constraint | Result |
|---|---|
| Small flash footprint (target: library code 32 KB or less) | The whole M7 test program, SDK included, is 39 KB |
| RAM: 520 KB shared with everything else | About 22 KB for two DIRECT screens; a BUF8 framebuffer adds width x height bytes |
| C on the Pico SDK; LVGL-like names | `qg_module_verb()`, `qg_xxx_t`, `QG_UPPER` throughout |

## 2. Architecture

```
Application
   │
Public API        qg_screen · qg_draw (+_pct) · qg_text · qg_image · qg_palette
   │
Surface backend   DIRECT (straight to glass)  |  BUF8 (8-bit framebuffer + flush)
   │
Panel drivers     ST7789 · ILI9341 · ST7796S  (table-driven init, shared MIPI DCS code)
   │
HAL               shared SPI bus, DMA (blocking and streamed), GPIO, backlight PWM

Asset pack reader (assets/): separate; hands out pointers into flash
```

- **One gateway for drawing.** Every shape reaches the backend through `qg_int_fill_rect()`, which is also where clipping happens, so clipping can't be forgotten.
- **Backends are a table of function pointers.** A backend leaves NULL any operation it doesn't support: BUF8 has no `write_rgb565`, and DIRECT has none of the framebuffer operations. Callers check, and pick another path or report `QG_ERR_UNSUPPORTED`.
- **The graphics library never knows where assets come from.** It only ever receives pointers and sizes.

## 3. Screens and the bus

- **Shared wiring.** SCK, MOSI, DC and RST are shared; each screen has its own CS. `qg_bus_init()` drives every CS high before the clock starts, and performs the one hardware reset.
- **Init sequence.** Each init step is its own CS transaction: CS must rise between commands (a lesson from M0). A software reset is sent only when the bus has no RST pin.
- **Per-screen SPI speed.** The HAL switches speed when the bus changes hands. All three boards run at 37.5 MHz on a breadboard.
- **Chip versus panel.** Chip facts (init table, RAM size) live in the driver; panel facts (size, offsets, `bgr`, `invert`, `mirror_x`/`mirror_y`) live in the screen config. Rotation is a MADCTL setting, and panel offsets are recalculated for every rotation.
- **Backlight.** PWM at 10 kHz with 100 brightness steps. Backlight pins that share a PWM slice are only initialised once, so they don't switch each other off.

## 4. Colour

- **Palettes.** Each screen has its own 256-entry RGB565 palette, copied from the standard one:

  | Entries | Contents |
  |---|---|
  | 0 to 15 | QuickBasic colours |
  | 16 to 231 | 6x6x6 colour cube |
  | 232 to 254 | grey ramp |
  | 255 | reserved: transparent |

- **Special colour values.** `qg_color_t` is 16 bits, so it can also hold `QG_TRANSPARENT` (255), `QG_DEFAULT` (0x100, the screen's foreground or background colour) and `QG_NONE` (0xFFFF, "can't read").
- **RGB lookup.** `qg_color_from_rgb()` finds the nearest entry, with the channels weighted 2/4/3 for the eye's sensitivity. It's meant for setup code, not per-pixel use.
- **Palette changes.** `qg_palette_set()` and `qg_palette_reset()` bump a per-screen generation counter, which invalidates cached colour matches, and mark a BUF8 screen entirely changed.

## 5. Coordinates

- **Pixels:** signed 16-bit, clipped everywhere.
- **Percentages:** integers 0 to 255 via `_pct` wrappers. 0 is the first pixel and 100 the last, rounding down. Radii are measured against the smaller dimension. Helpers `qg_pct_x()`, `qg_pct_y()` and `qg_pct_r()` let code mix both styles.

## 6. Drawing

- **Everything is spans.** Shapes are drawn as horizontal spans; lines as runs (Bresenham, with same-row or same-column pixels grouped).
- **Circles and ellipses** share one row-by-row engine. Its per-row half-width comes from an exact integer square root, and each row is drawn once as stroke, fill, stroke, so nothing is sent twice.
- **Line thickness** (added in M2; the rev 2 plan had deferred it) is a per-screen setting. Thick lines are filled rotated rectangles, centred on the line. Box, circle, ellipse and arc outlines grow inward, so a shape's outer size never changes.
- **Arcs** test each pixel's angle with integer cross products rather than trigonometry, and skip the rows the arc can't reach. This replaced the M2 `atan2` version and made arc slices 16 times faster on the hardware.
- **POINT and PAINT** work on BUF8 screens only. The flood fill is scanline-based, with a fixed work list (`QG_PAINT_STACK`), and returns `QG_ERR_OVERFLOW` rather than crashing on pathological shapes.

## 7. Text

- **Fonts** use LVGL's `fmt_txt` layout: 1 bit per pixel, uncompressed, with both kerning forms and all four character-map forms. A stand-in `lvgl/lvgl.h` lets LVGL-converted files compile unchanged. `tools/ttf2qg.py` produces the same layout offline. Three DejaVu fonts are included.
- **Font objects and slots.** A font object pairs font data with a default colour and scale (1 to 4). Screens hold 4 font slots; slot 0 is the default.
- **The engine** works in three stages: tokens (characters, escapes, markup), then line layout, then drawing. Layout comes first so that mixed fonts on one line share a baseline, and so that word wrap knows where lines end.
- **Markup:** `{c:}`, `{f:}`, `{s:}`, `{x:}`, `{{`. Unknown tags are skipped.
- **Escapes:** `\n`, `\r`, `\t`.
- **Word wrap:** at spaces and tabs; a word longer than a whole line is split.
- **Background:** transparent by default. Opaque text builds each character cell in RAM and sends it in one transfer, three times faster than erasing first.
- **Printing:** cursor printing with a margin set by `qg_locate`, plus `qg_print_at`, `qg_print_align`, `qg_print_box` and `qg_text_measure`.
- **Scrolling on DIRECT screens.** The last 32 printed lines are remembered as markup strings, each piece tagged with its exact starting state, and are reprinted one row higher. Tested pixel-exact. The line count must cover the tallest screen in the smallest font, which is why 32 lines were needed rather than the 16 first planned.
- **Scrolling on BUF8 screens** moves the pixels in RAM, with no memory of lines needed.

## 8. Images

- **Format:** 8-bit BMP, plain or RLE8, with per-image palettes, read in place from flash. RLE8 delta codes are supported.
- **Transparency:** palette index 255, when the image is opened with `QG_IMAGE_TRANSPARENT`.
- **Drawing:** rows are processed in stored order, so only one source row is ever held in RAM. Screen columns map to image columns through a table built once per draw by stepping rather than dividing; removing a per-pixel 64-bit division doubled drawing speed. On DIRECT screens rows are gathered into blocks for transfer; transparent images send runs of solid pixels.
- **On BUF8 screens,** image colours are matched to the screen's palette through a cached 256-entry table (4 tables kept). Matching happens after rounding to RGB565, so a colour copied into the palette with `qg_palette_load_image()` always matches exactly.
- **Converter:** `tools/img2bmp8.py` takes PNG, JPG, BMP or GIF; handles alpha and key-colour transparency and optional dithering; writes RLE8; and can output a C array.

## 9. Asset pack

- **Location:** a read-only pack at flash offset 1 MB, up to 3 MB. It's installed by UF2 drag-and-drop (the "absolute" family) or with `picotool load`, independently of the firmware.
- **Format:** a 32-byte header (magic, version, count, size, CRC-32), a table of 48-byte entries sorted by name (name, offset, size, type, flags), then the data, 4-byte aligned.
- **Builder:** `tools/mkpack.py` converts images automatically and flags transparency, with per-file options in `pack.txt`.
- **Reader:** `qg_asset_find()` binary-searches the table, taking 1.2 microseconds. `qg_asset_init()` validates every entry and refuses to run if the firmware has grown into the pack's area. `qg_asset_verify()` checks the CRC at about 0.2 seconds per MB.

## 10. Framebuffer (BUF8) screens

- **Memory:** the application provides the framebuffer, one byte per pixel.
- **Drawing** goes into RAM. `qg_screen_flush()` sends the rectangle that changed since the last flush, converting to RGB565 in chunks while DMA sends the previous chunk.
- **Flush speed:** a full 320x480 flush takes 72 ms against a 65.5 ms transfer, so about 14 full updates per second is the ceiling on that screen. Partial updates are proportionally faster.
- **What BUF8 adds:** `qg_point`, `qg_paint`, palette animation, memory-move scrolling, and `qg_palette_load_image` for exact image colours.

## 11. Threading readiness

Everything still runs on core 0. As designed:

- All state lives in screen, bus and font objects, apart from a few fixed work buffers.
- The flush is the natural hand-off point for a future core-1 renderer.
- The HAL's streamed transfers are a first step toward asynchronous drawing.

**Note for threading later:** the fixed work buffers (image rows, text cell, flush chunks, flood fill list) are shared, so drawing on two cores at once would need one set per core, or a lock.

## 12. Tools

| Tool | Purpose |
|---|---|
| `ttf2qg.py` | TrueType to font file |
| `img2bmp8.py` | Images to RLE8 BMP or C array |
| `mkpack.py` | Folder to asset pack UF2 or BIN |
| `size_report.py` | Flash and RAM per source file, from the build's map file |

## 13. Deferred, and possible next steps

- **Deferred:** GIF images; FAT or USB-drive asset loading; fonts loaded from the asset pack; clip regions and viewports; anti-aliased fonts; filled arcs (pie slices); a core-1 renderer; an RGB565 framebuffer.
- **Small, useful additions:** a region-limited flush for localised palette effects; overlapping image decoding with DMA; hyphen- and slash-aware word breaks.
- **Public release (M10):** generalise the names and demos, choose a name (working title QGP), and write a reference manual with an entry and a working example for every function, examples first and theory later.

## 14. Decision log (changes from rev 2)

| Topic | Rev 2 plan | As built |
|---|---|---|
| Line thickness | Deferred | Per-screen line width, from M2 |
| Arc pixel test | (unspecified) | Integer cross products, 16x faster than `atan2` |
| Text scrolling memory | 16 lines x 64 chars | 32 lines x 120 chars (covers 320x480 in the 12 px font) |
| Text boxes | `qg_print_align` | Plus `qg_print_box` (wrap and align in any column) |
| Screen colours | Palette only | Plus fg/bg defaults (`qg_screen_set_colors`, QuickBasic's COLOR) |
| Image column mapping | Per-pixel division | Stepped table, built once per draw |
| Images on BUF8 | Nearest-colour remap | Cached remap, plus `qg_palette_load_image` for exact colours |
| Framebuffer memory | Library-allocated | Application-provided (`cfg.framebuffer`) |
| Flush | Full frame | Changed-rectangle tracking, streamed DMA |
| Asset pack | Planned | Built as specified; UF2 "absolute" family confirmed working |
| MISO / chip-ID read | Planned for the 3.5" board | Dropped: symptoms identified the ST7796S; all buses are write-only |
