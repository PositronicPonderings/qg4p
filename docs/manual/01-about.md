# 1. About

QG4P draws on small SPI colour screens from a Raspberry Pi Pico 2, the way QuickBasic drew on a PC: `qg_cls`, `qg_pset`, `qg_line`, `qg_circle`, `qg_paint`, `qg_locate`, `qg_print`. Plus fonts, images, an asset pack, and flicker-free framebuffers when you want them.

It's written in C for the Pico SDK, and it's small: about 20 KB of flash for a typical program's share of it, and a few KB of RAM. It follows one rule throughout: **if you don't use a feature, you don't pay for it**, in memory, flash or time.

**What it runs on:**
- Raspberry Pi Pico 2 (RP2350). The original Pico (RP2040) should work, but hasn't been tested.
- Screens with an **ST7789**, **ILI9341** or **ST7796S** controller, on SPI: tested on 2.0" 240x320, 2.8" 240x320 and 3.5" 320x480 boards. Others can be added ([Part 8](08-new-chip.md)).
- Several screens on one SPI bus, each with its own chip, speed, rotation and backlight.

**What it looks like:**

```c
qg_cls(&scr, QG_BLUE);
qg_circle_pct(&scr, 50, 40, 30, QG_WHITE, QG_RED);
qg_print_align(&scr, 250, "Hello, {c:YELLOW}Pico{c:}!", QG_ALIGN_CENTER);
```

**Published by** [Positronic Ponderings](https://github.com/PositronicPonderings).

**Sources:** the bundled fonts are converted from the DejaVu fonts. The ILI9341 and ST7796S start-up sequences follow those published in Adafruit's and Bodmer's libraries.

**AI disclosure:** see [`AI_DISCLOSURE.md`](../../AI_DISCLOSURE.md).

**Licence:** MIT-0. Do what you like with it; no credit required. The fonts keep their own (also permissive) licence: see `LICENSE-fonts`.
