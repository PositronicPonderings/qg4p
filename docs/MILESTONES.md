# Milestone history

> Written during development, before the library was renamed QG4P. Names here are as they were then: `gfx_*` is now `qg_*`, `GFX_*` is `QG_*`, `demo/` is `tests/hardware/`, and the "DM" and "player" screens are screens A and B.

How the library was built, one milestone at a time. Each milestone ended with a test program in `demo/` and was checked on real hardware before the next began. The checklists below are what "pass" looked like; the notes and results record what was measured and what was found and fixed along the way.

Hardware throughout: Raspberry Pi Pico 2 (RP2350); 2.0" ST7789 240x320 (DM screen); 2.8" ILI9341 240x320 and 3.5" ST7796S 320x480 (player screen); all on one shared SPI bus at 37.5 MHz.

## M0 checklist

| Check | Pass looks like | If not |
|---|---|---|
| Serial shows the actual SPI speed | `actual 37500000 Hz` | Check the `PIN_SCK` and `PIN_MOSI` numbers |
| Test 1: first colour | Black | White means flip `PANEL_INVERT` |
| Test 1: colour 4 | Red | Blue means flip `PANEL_BGR` |
| Test 2: corners | Red top-left, green top-right, blue bottom-left, yellow bottom-right | Left/right swapped: flip `PANEL_MIRROR_X`. Top/bottom swapped: flip `PANEL_MIRROR_Y` |
| Test 2: border | All four white edges visible in every rotation | Check `x_offset`/`y_offset` |
| Test 2: magenta square | Left half visible at the right edge, nothing wrapped around | This would be a clipping bug; please report it |
| Test 3: speed | About 33 ms per clear at 37.5 MHz | Much slower means the DMA path isn't being used |
| Speckles or shifted pixels | None | Lower `SPI_HZ` to 20000000 (long breadboard jumpers) |

## M0 notes (lessons from bring-up)

- **CS must rise between init steps.** Holding CS low through the whole power-up sequence, including a software reset, left the ST7789 with a black screen and its backlight on. The controller resynchronises its serial receiver on the rising edge of CS, so the library now releases CS after every init step.
- **No redundant software reset.** The shared RST line resets every chip in `gfx_bus_init()`. A software reset is sent only when a bus has no RST pin.
- **37.5 MHz works** on the 2.0" ST7789 over breadboard jumpers.
- **Horizontal bands** (faint colour streaks along rows that contain bright blocks, fading after a few minutes of running) appear with both the library and the original POC. They're panel behaviour, not a data error.

## M1 checklist

The build's source file is set on the `add_executable` line of the top-level `CMakeLists.txt`. For M1 it's `demo/m1_demo.c`.

| Check | Pass looks like | If not |
|---|---|---|
| Serial shows both screens | Both at 37.5 MHz | "init FAILED" names the screen; check its CS wire |
| Test 1: colours | Both screens show the same colour at the same time | On the player screen: white for black means flip `PLAYER_INVERT`; red/blue swapped means flip `PLAYER_BGR` |
| Test 2: orientation | Same pattern as M0 on both screens, in every rotation | Player corners mirrored: flip `PLAYER_MIRROR_X` or `PLAYER_MIRROR_Y` |
| Test 3: brightness | Each backlight fades while the other stays steady; the DM screen returns at 10% after off/on | Both fading together means the backlight wires are swapped or joined |
| Test 4: hand-over | DM ends solid green, player solid blue, no stripes | Stripes on the player only: lower `PLAYER_SPI_HZ` |
| Optional: faster player | Set `PLAYER_SPI_HZ` to `40000000u` and rerun | If it breaks, go back to 20 MHz |
| Optional: 3.5" board | Set `PLAYER_BOARD` to `PLAYER_ST7796`, swap the board in, rerun Tests 1 and 2 | Settings are confirmed; see WIRING.md section 4 |

## M1 results

All tests passed with both player boards. Every board runs at 37.5 MHz on the breadboard. Confirmed panel settings for all three boards are in `docs/WIRING.md`, section 4.

## M2 checklist

Build line: `add_executable(dice_gfx demo/m2_demo.c demo/demo_setup.c)`. Choose the player board at the top of `demo/demo_setup.c`.

The demo cycles through six pages on both screens, printing what to look for and how long each page took.

| Page | Pass looks like |
|---|---|
| 1 Lines | Starburst with no gaps at the centre; widths 1 to 8 growing evenly |
| 2 Boxes | Columns: outline / filled / both. Rows: widths 1, 3, 6, all drawn inward so every box is the same outer size |
| 3 Circles | Round shapes, outlines hugging their fills with no gaps; even thick rings |
| 4 Arcs | Gauge fills clockwise, green to yellow to red; four coloured quarters form a circle; a thick three-quarter arc open at the top right |
| 5 Palette | Six RGB swatches, a smooth grey ramp, bar A **grey** and bar B **gold** (same palette index, changed between the two) |
| 6 Clipping | Shapes cut off cleanly at every edge; nothing wraps around |

## M3 checklist

Build line: `add_executable(dice_gfx demo/m3_demo.c demo/demo_setup.c)`.

| Page | Pass looks like |
|---|---|
| 1 Layout | The same mock dice-roller layout on both screens, upright and then turned 90 degrees. The yellow meter stays round around the die; white corner ticks touch each corner exactly |
| 2 Gauge | Serial shows the time per 10-degree slice. M2 took roughly 12 ms on the DM screen |
| 3 Dial | A yellow marker spins clockwise smoothly on both screens; serial shows frames per second |

**Arc change in M3:** arcs now decide which pixels are inside the angle range with integer cross products instead of `atan2()`, and skip rows the arc can't reach. Tested against the M2 version on 6,240 arcs: 205 pixels out of 2.87 million differ, never more than one per arc, all sitting exactly on an arc's end angle.

## M4 checklist

Build line: `add_executable(dice_gfx demo/m4_demo.c demo/demo_setup.c)`.

| Page | Pass looks like |
|---|---|
| 1 Fonts | Three fonts, clean and readable; scale 2; the symbols ° ± ×; descenders (g j p q y) dropping below the line; the euro sign shown as "?" |
| 2 Opaque | White text on the blue banner; both counters reach 200 cleanly with no leftover digits. Serial shows microseconds per update for each method |
| 3 Cursor | "Roll log" heading, an indented list (20 green, 1 red), "Two lines / from one call" aligned with the list, HP in the top-right corner, a big centred number in a yellow box |

## M5 checklist

Build line: `add_executable(dice_gfx demo/m5_demo.c demo/demo_setup.c)`.

| Page | Pass looks like |
|---|---|
| 1 Markup | Coloured words by name and number; "BIG" and the big green "20" sitting on the same baseline as the small text around them; mono and bold words mid-line; "{like this}" printed with its brace and the nonsense tag invisible; cyan carrying over a line break, then plain text on the next call |
| 2 Wrap | The story wrapped at word boundaries, with the red words in place; a centred and a right-aligned line; a blue box that fits its wrapped, centred message. Serial shows the measured size |
| 3 Tabs | A 4-column table lined up with tabs; two rows lined up with `{x:}` |
| 4 Scroll | 40 log lines: the screen scrolls smoothly line by line, grey labels and coloured results intact, with no gaps at the top. Serial shows the time per line |

**Scrolling memory:** DIRECT screens remember up to `GFX_TEXT_HISTORY_LINES` printed lines (32 by default, about 4 KB of RAM per screen) and reprint them one row up to scroll. Only text from `gfx_print`/`gfx_println` is remembered; shapes and `gfx_print_at` text are cleared by a scroll. Tested pixel-exact: after 40 lines, both screen sizes match an unscrolled reference exactly, with transparent and opaque text.

**Braces:** from M5 on, `{` starts markup. Write `{{` for a literal brace.

## M6 checklist

Build line: `add_executable(dice_gfx demo/m6_demo.c demo/demo_setup.c demo/demo_images.c)`.

| Page | Pass looks like |
|---|---|
| Serial | All five images listed as "OK" when opened |
| 1 1:1 | Banner and landscape; the left d20 on the checkerboard with the squares showing through its corners; the right d20 on a magenta square (flag deliberately left off) |
| 2 Scaling | Crisp, blocky potions at 1, 2, 4 and 8 times; a wide and a tall d20; the landscape at half size |
| 3 Fit | The landscape never distorted: left, centre and right in the wide boxes, centred in the tall and square ones |
| 4 Speed | Serial shows each draw's time; d20s cut off cleanly at all four corners (the stretched landscape and oval sun are intended) |

**Tested against Pillow:** 78 draws of RLE8 and plain versions of the test images (1:1, enlarged, shrunk, stretched, clipped) match Pillow's decoding exactly, with correct coverage and no pixel sent twice. RLE8 "delta" codes, which the converter never writes, were checked by hand, because Pillow's own delta handling has a bug.

## M7 checklist

Top-level `CMakeLists.txt`:
```cmake
add_subdirectory(gfx)
add_subdirectory(assets)
add_executable(dice_gfx demo/m7_demo.c demo/demo_setup.c)
target_link_libraries(dice_gfx pico_stdlib gfx assets)
```

| Step | Pass looks like |
|---|---|
| Run BEFORE loading the pack | Both screens say "No asset pack" with instructions |
| Build the pack | `mkpack.py` lists 6 files, about 16 KB |
| Load it (drag or picotool) | The Pico restarts into the demo |
| Serial | Firmware size (well under 1024 KB), "Asset pack: OK", "Checksum: OK", and the lookup time |
| 1 Contents | DM lists the 6 files (sizes, types, which are transparent); player shows the welcome text |
| 2 Images | Banner, landscape, two d20s and a potion: all from the pack, none compiled in |
| 3 Errors | "not found" for d21 and for "Dice/" (capital D); OK for dice/d20.bmp |

**Tested on the PC:** the reader finds all six files, each byte-identical to the M6 arrays and 4-byte aligned; the checksum passes, and catches a single flipped bit; a bad table entry, erased flash and a newer pack version are all refused cleanly. The UF2 reassembles to exactly the `.bin` at the right addresses.

**M7 results (hardware):** firmware 39 KB; pack 16.8 KB (its UF2 is 33.8 KB: UF2 blocks carry 256 data bytes each, so a UF2 is always about twice its payload); checksum 3.6 ms for 16.8 KB, about 0.2 s per MB; `asset_find` 1.2 µs per lookup; drag-and-drop loading works.

## M8 checklist

Build line: `add_executable(dice_gfx demo/m8_demo.c demo/demo_setup.c demo/demo_images.c)`. The DM screen stays DIRECT and the player becomes BUF8.

| Page | Pass looks like |
|---|---|
| Serial | Player listed as `BUF8`; the time of one full-screen flush |
| 1 Flicker | Bouncing dice flicker on the DM screen and are solid on the player, including where dice overlap. Serial compares time per frame |
| 2 Full scene | Player redraws landscape, dice and a frame counter every frame with no flashing; landscape colours exact. Serial splits draw and flush time |
| 3 Paint | Circle and box regions fill one by one in different colours, then the background turns grey; "POINT: BLUE, WHITE". DM reports GFX_ERR_UNSUPPORTED and GFX_NONE |
| 4 Palette | Rainbow rings flow outward smoothly; nothing is redrawn. Serial gives frames per second |
| 5 Scroll | Both logs scroll; serial compares the times |

**Tested on the PC**, with the real BUF8 backend flushing into a simulated panel:
- A scene of shapes, arcs, thick lines, markup, opaque and wrapped text gives identical pixels on DIRECT and on BUF8 + flush.
- A flush sends exactly the changed rectangle (and nothing when unchanged); a palette change sends everything.
- `gfx_paint` matches an independent reference fill exactly in border and bucket modes; a pathological maze reports `GFX_ERR_OVERFLOW` instead of crashing.
- BUF8 scrolling after 40 lines matches the unscrolled reference exactly on both screen sizes.
- Images cover exactly the same pixels as on DIRECT; colours are within one palette step on the standard palette (worst 24 of 255 per channel), and exact after `gfx_palette_load_image`.
- All earlier suites (M5 scrolling, M6 images, M7 pack) still pass.

**M8 results (hardware, ST7796S 320x480 as BUF8):**

| | Debug | Release |
|---|---|---|
| Full-screen flush | 71.8 ms | 71.8 ms |
| Flicker page, per frame (DM DIRECT / player BUF8 incl. flush) | 15.1 / 29.9 ms | 13.9 / 29.5 ms |
| Full scene: draw + flush | 11.9 + 71.8 ms (11 fps) | 10.3 + 71.8 ms (12 fps) |
| gfx_paint, per region | 1.4 to 4.3 ms | 1.2 to 3.5 ms |
| Palette animation (full flush) | 13 fps | 13 fps |
| 40-line log: DIRECT / BUF8 | 2014 / 1371 ms | 1875 / 1367 ms |

A full 320x480 flush moves 2.46 million bits; at 37.5 MHz that alone is 65.5 ms, so the flush runs at about 91% of the SPI link's capacity and can't gain from a Release build. Full-screen updates on this screen therefore top out near 14 fps; updates that touch only part of the screen are proportionally faster, and the library sends only the changed area automatically. (A 240x320 screen's full flush is half that: about 33 ms.)

Found on hardware: the first M8 demo erased and redrew each die in turn, so an overlapping die's erase painted over one already drawn (seen as a blue box). Fixed by erasing all, then drawing all; checked over 150 frames against a fresh redraw.
