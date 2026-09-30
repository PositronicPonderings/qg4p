# 8. Adding a new display chip

You've found the perfect screen: right size, right price, lovely viewing angle. Its listing names a controller chip QG4P doesn't know. This part is for you, and no, you don't need to be an electronics engineer. Most of the work is finding a list of numbers someone else has already found, and typing it in.

[What a controller chip is](#what-a-controller-chip-is) · [Will it work?](#will-it-work) · [1. Find the start-up sequence](#1-find-the-start-up-sequence) · [2. Write the driver](#2-write-the-driver) · [3. Tell QG4P about it](#3-tell-qg4p-about-it) · [4. Test it](#4-test-it) · [For driver authors: the hardware layer](#for-driver-authors-the-hardware-layer)

---

## What a controller chip is

A screen module is two things glued together: the **glass** (the panel you look at) and a **controller chip** behind it. The chip holds the picture in its own memory and keeps the glass lit with it. QG4P never talks to the glass; it sends the chip **commands** ("wake up", "use 16-bit colour", "draw here") and **pixels**.

The good news: most small colour-screen chips (ST7789, ILI9341, ST7796S, ST7735, ILI9488, GC9A01 and many more) share a common command language called **MIPI DCS**. The commands for "wake up", "draw here" and "turn on" are the same numbers on all of them. What differs from chip to chip is mostly the **start-up sequence**: a list of settings (voltages, timings, colour curves) the chip needs after power-up.

So a QG4P driver is small: that list, plus the size of the chip's memory. Everything else is shared.

---

## Will it work?

Likely, if the listing says:
- **SPI**, "4-wire SPI", or "serial" interface (not "8-bit parallel" only);
- **16-bit colour**, "RGB565" or "65K colours" among its modes;
- a MIPI DCS chip: ST77xx, ILI93xx, ILI94xx, GC9Axx, HX83xx families usually are.

**Not this way:** monochrome OLEDs (SSD1306 and friends), e-paper, and screens driven over HDMI or parallel RGB. Those work quite differently, and would need a different kind of backend, not just a driver.

**One catch to check:** some chips (ILI9488 is the famous one) accept only 18-bit colour over SPI, not 16-bit. QG4P sends 16-bit pixels, so those need more than a new start-up sequence.

---

## 1. Find the start-up sequence

In rough order of convenience:

1. **Open-source libraries that already support the chip.** Adafruit's display libraries, Bodmer's TFT_eSPI, LVGL's drivers, and MicroPython drivers all contain start-up sequences as lists of command and data bytes. They're the fastest route, because someone has already made them work.
2. **The seller's sample code**, often a zip file linked from the listing, or on a wiki that the listing points to.
3. **The chip's datasheet.** Look for "initialization sequence", "power on sequence" or a sample code appendix.

**Note where it came from.** A start-up sequence is a list of register settings, mostly from the manufacturer's datasheet, collected and tested by whoever published it. Say where yours came from in a comment, as QG4P's drivers do (the next person to debug it will want to compare), and check that library's licence.

You'll typically find something like this (from an Arduino library):

```c
writeCommand(0x11);  delay(120);          // sleep out
writeCommand(0x3A);  writeData(0x55);     // 16-bit colour
writeCommand(0xB2);  writeData(0x0C); writeData(0x0C); writeData(0x00); writeData(0x33); writeData(0x33);
...
```

Each line is one step: a **command** byte, some **data** bytes, sometimes a **pause**.

---

## 2. Write the driver

Copy an existing driver as your starting point: [`qg4p/drivers/qg_drv_st7789.c`](../../qg4p/drivers/qg_drv_st7789.c) is the shortest. Say your chip is the imaginary "XY1234", 240x320:

```c
/* SPDX-License-Identifier: MIT-0 */
/* XY1234 driver. Start-up sequence from <where you found it>. */
#include "drivers/qg_driver.h"

static const qg_init_cmd_t xy1234_init[] = {
    /* command         bytes  pause   data                               */
    { QG_CMD_SLPOUT,     0,   120,    { 0 } },                        /* wake up    */
    { QG_CMD_COLMOD,     1,    10,    { 0x55 } },                     /* 16-bit     */
    { 0xB2,              5,     0,    { 0x0C, 0x0C, 0x00, 0x33, 0x33 } }, /* from the source */
    { QG_CMD_NORON,      0,    10,    { 0 } },                        /* normal mode */
};

const qg_driver_t qg_drv_xy1234 = {
    .name         = "XY1234",
    .init_seq     = xy1234_init,
    .init_count   = (uint8_t)(sizeof(xy1234_init) / sizeof(xy1234_init[0])),
    .ram_w        = 240,          /* the chip's memory, upright (not the glass) */
    .ram_h        = 320,
    .max_write_hz = 0,            /* datasheet speed if you know it; 0 if not   */
};
```

**Translating the sequence you found:** each `writeCommand(C)` followed by `writeData(D1)`, `writeData(D2)`... becomes `{ C, number of data bytes, pause in ms, { D1, D2, ... } }`. A `delay(120)` after a step becomes its pause. At most 16 data bytes per step; longer steps are rare, and can usually be split where the source does.

**Leave these out**, because QG4P sends them itself, from the screen's config:
- **software reset** (`0x01`): the shared reset wire does it, or QG4P sends one if there's no reset wire;
- **inversion on/off** (`0x20`, `0x21`): from `.invert`;
- **orientation** (`MADCTL`, `0x36`): from `.rotation`, `.mirror_x`, `.mirror_y` and `.bgr`;
- **display on** (`0x29`): sent last, after the screen is cleared, so you never see the memory's random start-up garbage.

**Chip memory vs glass:** `ram_w` and `ram_h` are the chip's memory size, from its datasheet. Often it's the same as the glass. When the glass is smaller (a 240x240 panel on a 240x320 chip, say), the screen's config gives the glass's size in `.width` and `.height`, and where it starts in `.x_offset` and `.y_offset`.

---

## 3. Tell QG4P about it

Three small edits:

1. **Add the file to the library's build**, in [`qg4p/CMakeLists.txt`](../../qg4p/CMakeLists.txt), next to the other drivers:
   ```cmake
   drivers/qg_drv_xy1234.c
   ```
2. **Give it a name programs can use**, in [`qg4p/qg_types.h`](../../qg4p/qg_types.h), next to the others:
   ```c
   extern const struct qg_driver qg_drv_xy1234;
   #define QG_DRIVER_XY1234   (&qg_drv_xy1234)
   ```
3. **Use it**, in a screen's config: `.driver = QG_DRIVER_XY1234`.

Programs that don't name your driver don't carry it; the name is a reference to the driver itself, so only programs that mention it pull it in.

---

## 4. Test it

1. **Start slow.** Set `.spi_hz = 1000000u` (1 MHz) until it works; speed comes later.
2. **Something on the screen?** Put the chip in `examples/board.h` and load `qg4p_hello`.
   - Still black: recheck the wiring, then the sequence: a mistyped byte in the power settings is the usual culprit. Compare step by step with your source.
   - Random coloured snow that never changes: the chip hasn't woken; check the sleep-out pause and the reset wiring.
3. **Right colours, right way round?** Load `qg4p_colour_check` and read [Help, my red looks blue](06-troubleshooting.md#help-my-red-looks-blue). Each of `invert`, `bgr`, `mirror_x` and `mirror_y` is a yes/no; if one is wrong, flip it. Corner labels in the wrong corners are mirroring; a strip of garbage along an edge means offsets.
4. **All four rotations?** `qg4p_layout` turns the screen every few seconds. If sideways is wrong but upright is right, check `ram_w` and `ram_h`, and the offsets.
5. **Speed.** Raise `.spi_hz` in steps until the picture shows speckles, then back off a step. On a breadboard, 37.5 MHz (`40000000u`) is typical for these boards; short soldered wires often do better.

When it works, a pull request with your driver (and the source of its sequence) helps the next person with the same screen.

---

## For driver authors: the hardware layer

Drivers normally need nothing but their table: `qg_screen_init` runs it. For a chip that needs something unusual at start-up, these are the building blocks QG4P itself uses (`qg4p/hal/qg_hal.h`, `qg4p/drivers/qg_driver.h`):

| Function | Does |
|---|---|
| `qg_hal_begin(dev)`, `qg_hal_end(dev)` | select the screen (CS low) and release it; everything in between is one conversation |
| `qg_hal_write_cmd(dev, cmd)` | send one command byte |
| `qg_hal_write_data(dev, bytes, n)` | send data bytes for the last command |
| `qg_hal_write_pixels(dev, pixels, n)` | send RGB565 pixels (by DMA) |
| `qg_hal_fill_pixels(dev, colour, n)` | send one colour n times |
| `qg_hal_stream_begin/pixels/end(dev, ...)` | send pixels in chunks, preparing the next while the last is sent |
| `qg_driver_run_init(dev, drv)` | run a driver's table (each step in its own conversation) |
| `qg_driver_set_window(dev, x0, y0, x1, y1)` | choose where the next pixels land |

`dev` is the screen's `&scr.dev`. One rule matters more than the rest: **chip select must rise between start-up commands**. Some chips ignore everything otherwise, and wake up to a black screen with the backlight cheerfully on. `qg_driver_run_init` does this for you.
