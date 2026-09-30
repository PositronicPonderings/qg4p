# Appendices

[A. Error codes](#error-codes) · [B. Settings](#settings) · [C. The markup card](#the-markup-card) · [D. Memory and speed](#memory-and-speed) · [E. Glossary](#glossary)

---

## Error codes

Functions that can fail return one of these (`qg_err_t`, in `qg_types.h`):

| Code | Value | Means | Typically from |
|---|---|---|---|
| `QG_OK` | 0 | it worked | |
| `QG_ERR_ARG` | -1 | a bad argument: a missing pointer, a bad pin, a size out of range, a point off the screen | `qg_bus_init`, `qg_screen_init`, `qg_image_open`, `qg_get`, `qg_paint` |
| `QG_ERR_UNSUPPORTED` | -2 | can't be done here: needs a framebuffer screen, or an image or font format QG4P doesn't draw | `qg_point`, `qg_paint`, `qg_get`, `qg_put`, `qg_image_open`, `qg_font_check` |
| `QG_ERR_OVERFLOW` | -4 | ran out of working space; the job is incomplete | `qg_paint` |

The asset pack reader has its own (`qg_asset_err_t`, in `qg_asset.h`); [`qg_asset_err_str`](reference/assets.md#qg_asset_err_str) turns any of them into words:

| Code | Means |
|---|---|
| `QG_ASSET_OK` | it worked |
| `QG_ASSET_ERR_NO_PACK` | no pack in flash: never loaded, or erased |
| `QG_ASSET_ERR_VERSION` | the pack comes from a newer `mkpack.py` |
| `QG_ASSET_ERR_CORRUPT` | the pack is damaged: bad sizes, or a checksum mismatch |
| `QG_ASSET_ERR_NOT_FOUND` | no file with that name |
| `QG_ASSET_ERR_OVERLAP` | the program has grown into the pack's space |
| `QG_ASSET_ERR_NOT_READY` | `qg_asset_init` hasn't succeeded |

Functions that return a colour return `QG_NONE` when there isn't one to give (`qg_point` on a DIRECT screen, or off the screen).

---

## Settings

Sizes and limits, in `qg4p/qg_config.h` (and `QG_ASSET_PACK_OFFSET` in `qg_asset.h`). Change them from your `CMakeLists.txt`, on the library and marked `PUBLIC`, so the library and your program agree:

```cmake
target_compile_definitions(qg4p PUBLIC QG_TEXT_CELL_PIXELS=0 QG_PAINT_STACK=2048)
```

| Setting | Default | Controls |
|---|---|---|
| `QG_BUS_BOOT_HZ` | 1 MHz | the SPI speed while screens start up (they then switch to their own `spi_hz`) |
| `QG_BL_PWM_HZ` | 10 kHz | the backlight dimming frequency: too fast to see, slow enough for the boards' switching transistors |
| `QG_MAX_FONTS` | 4 | font slots per screen |
| `QG_TEXT_CELL_PIXELS` | 2048 | the opaque-text buffer (2 bytes a pixel: 4 KB). Characters bigger than this draw a slower way. **0 removes it** |
| `QG_TEXT_HISTORY_LINES` | 32 | a suggested size for your text-history arrays (enough for any screen in the smallest font). The memory itself is yours: see [scrolling](reference/text.md#scrolling) |
| `QG_TEXT_HISTORY_CHARS` | 120 | how much markup one remembered line holds; each line costs this + 6 bytes |
| `QG_TAB_WIDTH` | 40 | pixels between tab stops, until `qg_screen_set_tab_width` |
| `QG_IMAGE_MAX_WIDTH` | 480 | the widest image QG4P can draw (sizes its row buffers) |
| `QG_IMAGE_BLOCK_PIXELS` | 2048 | how many image pixels are gathered per transfer on DIRECT screens (2 bytes each) |
| `QG_BUF8_CHUNK_PIXELS` | 1024 | the flush's chunk size; two chunks of 2 bytes a pixel (4 KB) |
| `QG_PAINT_STACK` | 1024 | `qg_paint`'s work list, 4 bytes an entry. Raise it if a very intricate shape returns `QG_ERR_OVERFLOW` |
| `QG_ASSET_PACK_OFFSET` | 1 MB | where in flash the asset pack lives. Must match `mkpack.py --offset` |

Each buffer only exists in programs that use its feature; see [memory](#memory-and-speed).

---

## The markup card

| In text | Does |
|---|---|
| `\n` | new line, back to the left margin |
| `\r` | back to the left margin, same line |
| `\t` | next tab stop |
| `{c:RED}` · `{c:200}` | colour by name (any case) or palette number |
| `{f:1}` | font slot 1 (0 to 3) |
| `{s:2}` | scale 2 (1 to 4) |
| `{x:120}` | jump to pixel column 120 |
| `{c:}` · `{f:}` · `{s:}` | back to this print's default |
| `{{` | a literal `{` |

The 16 colour names: BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY, DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE.

---

## Memory and speed

**Memory.** Typical programs, RAM used by QG4P:

| Program | RAM |
|---|---:|
| One DIRECT screen, shapes and text, no scrolling | about 5 KB |
| Two DIRECT screens, text and images, scrolling on one | about 17 KB |
| The same, with one screen a 320x480 framebuffer | about 172 KB |

The Pico 2 has 520 KB. Flash: QG4P's code is about 20 KB in a typical program, plus fonts (1.5 to 3.5 KB each). The full breakdown, every buffer and what it's for, is in [`docs/SIZES.md`](../SIZES.md); [`size_report.py`](05-tools.md#size_reportpy) measures your own program.

**Speed.** Measured on a Pico 2, SPI at 37.5 MHz:

| Operation | Time |
|---|---|
| Clear a 240x320 screen | 36 ms |
| Full-screen flush, 320x480 framebuffer | 72 ms (the wire alone: 65.5 ms) |
| A page of mixed shapes, 240x320 | 15 to 50 ms |
| A 10-degree slice of a thick gauge arc | 0.74 ms |
| Update a number, opaque text | 1.2 ms (erase and redraw: 3.6 ms) |
| Scroll one line of text, DIRECT, 240x320 | 36 ms |
| A 240x160 image, 1:1 | 26 ms |
| Three moving sprites on a 320x480 framebuffer, with flush | 30 ms a frame |
| `qg_paint`, one region | 1.2 to 3.5 ms |
| `qg_asset_find` | about 1 microsecond |

Most of these are set by the SPI wire, not the processor: a Release build is at most 20% faster than Debug.

---

## Glossary

| Term | Means |
|---|---|
| **Backend** | where a screen's drawing goes: DIRECT (straight to the glass) or BUF8 (a framebuffer) |
| **Backlight** | the light behind an LCD panel; without it you see nothing, however good the picture |
| **BGR** | a panel whose colour order is blue-green-red, not red-green-blue; if red and blue look swapped, flip `bgr` |
| **Bitmap font** | a font stored as small pictures of each character (as QG4P's are), rather than outlines |
| **BOOTSEL** | the button on the Pico that, held while plugging in, makes it appear as a drive for loading programs |
| **BUF8** | QG4P's framebuffer backend: one byte (8 bits) per pixel |
| **Clipping** | cutting drawing off at the edge of the screen (or view) |
| **CMake** | the tool that reads `CMakeLists.txt` and works out how to build a program |
| **Controller chip** | the chip on a screen board that holds the picture and drives the glass |
| **CS (chip select)** | a wire telling one screen "this is for you"; one per screen |
| **DC (data/command)** | a wire saying whether the bytes on the bus are a command or data |
| **DMA** | direct memory access: hardware that moves data (pixels to the SPI bus) without the processor |
| **Dithering** | scattering pixels of available colours to fake ones that aren't available |
| **Flash** | the Pico's non-volatile memory: programs, fonts, images, the asset pack (4 MB on a Pico 2) |
| **Flush** | sending a framebuffer's changes to the panel |
| **Framebuffer** | a copy of the whole screen in RAM, drawn on invisibly and then flushed |
| **Gamma** | the curve relating a colour's number to how bright it looks |
| **GPIO / GP number** | a general-purpose pin, and the number code uses for it (GP18), not the physical pin number |
| **IPS / TN** | two kinds of LCD panel: IPS keeps its colours at an angle; TN is cheaper and doesn't |
| **Kerning** | tucking particular letter pairs closer together ("AV") |
| **Linker** | the tool that assembles a program from compiled pieces, leaving out what isn't used |
| **Map file** | the linker's record of everything it placed, and how big it is |
| **MIPI DCS** | the standard command set most small colour-screen chips share |
| **MOSI / SCK** | SPI's data-out and clock wires |
| **Palette** | a list of colours that pictures refer to by number |
| **PWM** | pulse-width modulation: switching something on and off fast to set an average level, here brightness |
| **RAM** | working memory, lost at power-off (520 KB on a Pico 2) |
| **RGB565** | a colour in 16 bits: 5 for red, 6 for green, 5 for blue; what these screens use |
| **RLE8** | run-length encoding for 8-bit images: runs of identical pixels stored as (count, colour) |
| **SPI** | serial peripheral interface: the clock-and-data connection to the screens |
| **Sprite** | a small image that moves over a background |
| **UF2** | the file format the Pico accepts by drag-and-drop |
| **View** | QG4P's (and QuickBasic's) rectangle that drawing is restricted to |
| **XIP** | execute in place: the Pico runs programs, and reads assets, straight from flash |
