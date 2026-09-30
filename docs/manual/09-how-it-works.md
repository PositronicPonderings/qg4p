# 9. How it works

For when you want to know why, not just how. Nothing here is needed to use QG4P; everything here is in the source too, in the comments, which are deliberately long.

[The layers](#the-layers) · [Paying only for what you use](#paying-only-for-what-you-use) · [One gateway for pixels](#one-gateway-for-pixels) · [Lines](#lines) · [Thick lines](#thick-lines) · [Circles and ellipses](#circles-and-ellipses) · [Arcs](#arcs) · [Flood fill](#flood-fill) · [Text](#text) · [Scrolling](#scrolling) · [Images](#images) · [Palettes and colour](#palettes-and-colour) · [The flush](#the-flush) · [The asset pack](#the-asset-pack) · [The SPI bus](#the-spi-bus)

---

## The layers

```
Your program
   │
Public API        qg_screen · qg_draw (+ _pct) · qg_block · qg_text · qg_image · qg_palette
   │
Backend           DIRECT (straight to the glass)   |   BUF8 (framebuffer + flush)
   │
Drivers           ST7789 · ILI9341 · ST7796S  (start-up tables + shared MIPI DCS code)
   │
Hardware layer    the shared SPI bus, DMA, GPIO, backlight PWM

The asset pack reader (qg4p_assets) sits beside all this: it only hands out pointers.
```

Drawing code never talks to hardware; it hands rectangles and pixels to the screen's **backend**. The DIRECT backend sends them to the panel at once; the BUF8 backend writes them into RAM and sends them at the next flush. That one split is why every drawing function works on both kinds of screen unchanged.

---

## Paying only for what you use

A C program for the Pico is **linked**: the linker gathers the pieces it needs from the libraries and leaves the rest. The Pico SDK builds with each function and variable in its own section (`-ffunction-sections -fdata-sections`) and links with `--gc-sections`, so an unused function, and any buffer only it uses, simply isn't there. `qg_paint`'s 4 KB work list only exists in programs that call `qg_paint`.

The subtle part is *references*. If any linked code names something, it's linked, whether it runs or not. So QG4P avoids naming optional things itself:
- `QG_BACKEND_BUF8` and `QG_DRIVER_ST7789` are references to the backend and driver **themselves**, written in your code. Screen set-up never names them, so a program carries only the ones it mentions. (An early version picked them from a table, which named every one of them, and so every program carried all of them: the size audit found it.)
- Memory some features need, text history and colour-adjustment tables, is memory **you** declare, sized as you choose. A screen that doesn't scroll or isn't calibrated carries none.

`tools/size_audit.py` checks a whole build: a library file showing up where it isn't used means something is naming it.

---

## One gateway for pixels

Every shape, one way or another, ends in `qg_int_fill_rect`: fill this rectangle with this colour. That's the single place where the [view](reference/drawing.md#qg_view)'s origin is added and everything is clipped to the screen (or view). A new shape can't forget to clip, because it can't reach the backend any other way. Text and images, which send blocks of pixels, clip once per block, not per pixel.

---

## Lines

Thin lines use **Bresenham's algorithm** (1962), which steps from one end to the other choosing, at each step, the pixel nearest the true line, using only integer additions: no division, no floating point. QG4P adds one refinement: consecutive pixels in the same row (or column, for steep lines) are grouped into **runs**, and each run is sent as one rectangle. A long shallow line is a handful of transfers, not hundreds.

**Dashed lines** walk the same path, checking the style pattern's bit for each step; each "on" stretch becomes a run.

---

## Thick lines

A thick line is a **rotated rectangle**: its four corners are the ends pushed out sideways by half the width, and extended by half a pixel at each end so the line covers exactly its end pixels. The rectangle is filled row by row (a convex polygon fill: for each row, find where the edges cross it, fill between). A thick dash is a shorter rotated rectangle, pointing the same way as the whole line; a one-pixel dash is one pixel long, not a square, which is why thick dotted lines stay dotted.

---

## Circles and ellipses

Circles and ellipses are drawn **row by row**. For each row, the ellipse's half-width there comes from an exact integer square root of `rx²(1 − y²/ry²)`. Then the row is at most three runs: left outline, fill, right outline. Nothing is drawn twice, and rows that can't be seen (above or below the screen or view) are skipped entirely.

A thick outline is the difference between two such shapes: the outer one, and the inner one shrunk by the line width. That's why thick outlines grow **inward**, and a shape's outer size never changes with its line width.

---

## Arcs

The obvious way to draw an arc is to work out each pixel's angle with `atan2` and keep the ones between the start and end angles. The first version of QG4P did exactly that, and arcs were 16 times slower than everything else.

The trick: whether a point lies between two angles doesn't need the angle at all. The start and end angles give two direction vectors (one `cos` and `sin` each, computed once per arc). A **cross product** (two multiplications and a subtraction) says which side of each direction a pixel is on, and two such signs decide "inside the arc". Rows the arc can't reach are skipped using the same test on the row's extent. Measured on the hardware: a thick 10-degree gauge slice went from 12 ms to 0.74 ms.

---

## Flood fill

The simple flood fill ("fill this pixel, then do the same for its four neighbours") is elegant, and remembers every pixel still to visit, which can mean tens of thousands. QG4P uses the **scanline** method:

1. Take a starting point from the to-do list.
2. Walk left and right from it to find the whole run of fillable pixels on that row, and fill it in one go.
3. Look along the rows just above and below that run. Each separate stretch of fillable pixels there gets **one** new entry on the to-do list.
4. Repeat until the list is empty.

The list holds one entry per stretch still waiting, so 1,024 entries (`QG_PAINT_STACK`) cover any ordinary shape. A pathological one (a checkerboard maze of single pixels) can run out; then the fill stops early and says so (`QG_ERR_OVERFLOW`) rather than crashing.

---

## Text

Printing happens in three stages:
1. **Tokens.** The text is decoded from UTF-8 into characters, escapes (`\n`, `\t`) and markup (`{c:RED}`), each with the style (font, colour, scale) in force.
2. **Layout.** Tokens are placed on lines: widths measured with kerning (letter pairs like "AV" tucked closer), words wrapped at spaces, tabs resolved to stops. A line's height is its tallest font, and every font on the line shares one baseline, so mixed sizes line up along their bottoms, not their tops.
3. **Drawing.** Each character's bitmap is sent: as individual runs for transparent text, or, for opaque text, assembled into one block in RAM (background and all) and sent in a single transfer, three times faster than erasing first.

Layout comes before drawing so that alignment and wrapping know where lines end before anything is drawn. `qg_text_measure` is the same process, stopped after stage 2.

Fonts use LVGL's format (1 bit per pixel), so each glyph is a compact bitmap plus its size and offset. Characters are found by code point through the font's character maps, a binary search in the common case.

---

## Scrolling

A framebuffer screen scrolls by moving its bytes up in RAM: one `memmove`, and a flush.

A DIRECT screen can't move pixels already on the glass, and can't read them back. So as it prints, it remembers each line as **markup that recreates it**, prefixed with the exact style it started in (`{~:...}`, an internal tag). To scroll, it clears the screen and reprints the remembered lines one row higher. That's why only text scrolls on a DIRECT screen, and why it needs [text history](reference/text.md#scrolling). It's tested pixel for pixel against the same text printed on a taller screen without scrolling.

---

## Images

Images are 8-bit BMPs with their own palettes, read where they sit in flash. **RLE8** compression stores runs of identical pixels along each row as (count, colour) pairs, with an escape for stretches of different pixels, and codes for "end of row" and "skip ahead". It suits flat art and horizontal runs, and does nothing for photographs; `img2bmp8.py` keeps whichever form is smaller.

BMPs usually store their rows bottom-up, so images are decoded one row at a time in stored order, and each finished row is placed wherever scaling says it goes. Only one source row is ever held in RAM.

**Scaling** is nearest-neighbour: each screen column shows one image column. Which one is worked out once per draw into a table, by stepping (Bresenham again) rather than dividing, because the RP2350 divides 32-bit numbers in hardware but 64-bit ones in software; removing one per-pixel 64-bit division doubled image drawing speed.

---

## Palettes and colour

Every screen keeps a palette: 256 entries of RGB565 (5 bits red, 6 green, 5 blue: the panels' native format). Drawing works in palette numbers, and colours become RGB565 only when they're sent: one table lookup per rectangle, or per pixel for image rows and flushes.

**Finding the nearest colour** (`qg_color_from_rgb`, and images on framebuffer screens) compares weighted distances, with green counting most and blue least, roughly as the eye does. For images on framebuffer screens, each image's 256 colours are matched to the screen's palette once, and the resulting table is cached (and invalidated when the palette changes).

**Colour adjustment** is three 256-entry lookup tables (one per channel), built once from the gain and gamma, and a second copy of the palette with the tables applied. Everything that sends colours reads that copy instead; nothing else changes, so it costs nothing per pixel.

---

## The flush

A framebuffer holds palette numbers; the panel wants RGB565. So a flush converts, a chunk at a time, into one of two buffers, and while DMA sends one buffer to the panel, the processor fills the other:

```
CPU:  convert A | convert B | convert A | convert B | ...
DMA:            |  send A   |  send B   |  send A   | ...
```

Conversion (one lookup per pixel) is quicker than sending, so it hides almost entirely behind the transfer. Every write to the framebuffer stretches a **changed rectangle**, and the flush sends only that, then forgets it.

The limit is the wire. A 320x480 screen is 153,600 pixels of 16 bits: 2.46 million bits, 65.5 ms at 37.5 MHz. Measured, a full flush takes 72 ms, about 91% of the wire's capacity. A sprite-sized change is a few milliseconds.

---

## The asset pack

The pack sits in flash at 1 MB (`QG_ASSET_PACK_OFFSET`), separately from the program:

```
header    32 bytes: "QGPK", version, file count, total size, CRC-32
entries   48 bytes each: name (36), offset, size, type, flags; sorted by name
data      each file, starting on a 4-byte boundary
```

Because the entries are sorted by name, `qg_asset_find` is a binary search: about a microsecond. `qg_asset_init` checks the header and every entry, and refuses a pack your program has grown into (it compares with the linker's end-of-program marker). `qg_asset_verify` checks the CRC over every byte, the slow part, so it's separate.

---

## The SPI bus

Screens on one bus share its clock, data, data/command and reset wires; each has its own chip select. Before the clock ever ticks, `qg_bus_init` drives **every** listed chip select high (inactive), so no screen hears traffic meant for another. Each screen keeps its own speed; the bus switches only when a different screen takes it over.

Start-up commands each get their own chip-select cycle. That was learned the hard way: during bring-up, holding chip select low through a reset left the ST7789 with a black screen and its backlight cheerfully on.
