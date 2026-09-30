# Memory and flash

The Pico 2 has 520 KB of RAM and, on a standard board, 4 MB of flash. This page lists what the library uses of each, and which settings change it.

## RAM

QG4P follows one rule: **if you don't use a feature, you don't pay for it.** Memory a feature needs is either linked only into programs that use that feature, or provided by you, sized as you choose.

### Fixed buffers inside the library

These are `static` arrays, sized by settings in `qg4p/qg_config.h`. The Pico SDK builds with `-ffunction-sections -fdata-sections` and links with `--gc-sections`, so each is included only if the program uses what needs it.

| Buffer | File | Bytes | Setting | Linked only when the program... |
|---|---|---:|---|---|
| Standard palette | `qg_palette.c` | 512 | (fixed) | uses any screen |
| Backlight PWM bookkeeping | `hal/qg_hal.c` | 4 | (fixed) | uses any screen |
| Opaque-text cell | `qg_text.c` | 4,096 | `QG_TEXT_CELL_PIXELS` (2048) | prints text. Set to 0 to remove it; opaque text then draws the slower way |
| Image row buffers | `qg_image.c` | 2,880 | `QG_IMAGE_MAX_WIDTH` (480) | draws images |
| Image transfer block | `qg_image.c` | 4,096 | `QG_IMAGE_BLOCK_PIXELS` (2048) | draws images |
| Image palette (RGB565) | `qg_image.c` | 512 | (fixed) | draws images |
| Flush chunks | `backend/qg_backend_buf8.c` | 4,096 | `QG_BUF8_CHUNK_PIXELS` (1024) | has a framebuffer screen (`QG_BACKEND_BUF8`) |
| Image colour-match cache | `backend/qg_backend_buf8.c` | 1,072 | (4 tables) | has a framebuffer screen |
| Flood-fill work list | `qg_draw.c` | 4,096 | `QG_PAINT_STACK` (1024) | calls `qg_paint()` |
| PUT row buffer | `qg_block.c` | 960 | `QG_IMAGE_MAX_WIDTH` (480) | calls `qg_put()` |
| Font lookup caches | `fonts/*.c` | 8 each | (fixed) | uses that font |
| CRC table | `assets/qg_asset.c` | 1,024 | (fixed) | calls `qg_asset_verify()` |
| **Everything, if all used** | | **23,372 (22.8 KB)** | | |

### Code linked only when named

The same rule applies to code. A program carries only the chip drivers it names (`QG_DRIVER_ST7789` and so on), the framebuffer backend only if it names `QG_BACKEND_BUF8`, and only the fonts it names. `tools/size_audit.py` shows, for a whole build, which library files each program includes.

### Memory you provide

The library never allocates these itself: you declare them, so only what you use exists.

| What | Bytes | Needed by |
|---|---:|---|
| `qg_screen_t` | 684 each | every screen |
| `qg_bus_t` | 28 | each SPI bus |
| `qg_font_t` | 8 | each font object |
| `qg_image_t` | 28 | each open image (the image data itself stays in flash) |
| Framebuffer | width x height | framebuffer (BUF8) screens only: 76,800 for 240x320, 153,600 for 320x480 |
| Text history, `qg_text_line_t[n]` | 126 per line | DIRECT screens that scroll text; 32 lines (4,032 bytes) covers any screen. Without it, printing past the bottom clears and starts again at the top |
| Colour adjustment, `qg_color_adjust_state_t` | 1,290 | calibrated screens only (`qg_screen_set_color_adjust`) |

(Sizes measured with a 32-bit compile, which matches the Pico's layout.)

### Typical totals

| Program | RAM used by the library |
|---|---:|
| One DIRECT screen, shapes and text, no scrolling | about 5 KB (1% of 520 KB) |
| Two DIRECT screens, text and images, scrolling on one (the dice roller, without framebuffer) | about 17 KB (3%) |
| The same, with screen B a 320x480 framebuffer | about 172 KB (33%) |
| The same, with screen B a 240x320 framebuffer | about 97 KB (19%) |

## Flash

Measured on the hardware: the whole M7 test program, including the graphics library, the three fonts, the asset reader, the demo and the Pico SDK, was **39 KB** of the 1 MB reserved for firmware. The asset pack has its own 3 MB after that.

| Item | Flash |
|---|---:|
| `qg_font_mono_12` | about 1.3 KB |
| `qg_font_sans_16` | about 1.8 KB |
| `qg_font_sans_bold_24` | about 3.4 KB |
| A 64x64 transparent icon (RLE8) | about 2.4 KB |
| A 240x160 flat-colour scene (RLE8) | about 1.8 KB |

### Measuring your own program

The build writes a map file listing every piece of code and data the linker placed. `tools/size_report.py` totals it per source file:

```
python3 tools/size_report.py build/my_app.elf.map
```

It prints flash and RAM for each file of `qg4p` and `qg4p_assets`, then totals for your program, the Pico SDK and the C library. Only what's actually linked is counted, so the numbers reflect the features your program uses.

### Checking every program at once

```
python3 tools/size_audit.py build
python3 tools/size_audit.py build --baseline old_sizes.json
```

`size_audit.py` finds every map file in the build (one per program), and lists each program's flash and RAM, then every library file with the programs that include it. That second list is the one for keeping the "don't pay for what you don't use" promise: a file where you don't expect it (say, `qg_backend_buf8`, the framebuffer, in a program without one) means some code is asking for it without needing it. It also saves everything as JSON, and `--baseline` compares a build against an earlier one: what grew, what shrank, and which library files came or went.
