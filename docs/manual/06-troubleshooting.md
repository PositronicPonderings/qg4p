# 6. Troubleshooting

Find your symptom. If you're reading this section you're probably stressed already, so: it's almost always something small, and it's almost always one of these.

[Black or blank screen](#the-screen-is-black-or-white) · [Help, my red looks blue](#help-my-red-looks-blue) · [Building](#building) · [Loading](#loading) · [Drawing and text](#drawing-and-text) · [Images and assets](#images-and-assets) · [Speed](#speed) · [Memory](#memory)

---

## The screen is black (or white)

**The backlight is on, but the screen is black.** Congratulations: you've built a very expensive flashlight. The panel has power but never heard a word you said. If you've ever been on hold with an airline, you know the feeling. Check, in this order:

1. **The wiring, by GPIO number.** The Pico's ground pins sit between the GPIOs, so counting along the header drifts by one. Every. Single. Time. See [the pin map](07-getting-started.md#physical-pins-and-gp-numbers).
2. **SCK and MOSI on the right pins** for the SPI you named. `qg_bus_init` returns `QG_ERR_ARG` if they aren't; check its result.
3. **The SPI speed.** Set `spi_hz` to `1000000u` (1 MHz). If the picture appears, your jumper wires are losing an argument with physics. Physics is undefeated. Shorten and reseat the wires, then creep back up.
4. **The driver.** An ST7796S configured as an ST7789 (or the other way round) usually stays black. Check the chip named in the listing, not just the board's size.
5. **CS.** Every screen on the bus must be listed in the bus's `cs_pins`, or an idle screen may join conversations it wasn't invited to.
6. **Power.** 3.3 V to VCC, and a shared ground. Many boards also take 5 V on VCC and regulate it down; check yours.
7. **Your own driver?** Chip select has to rise between start-up commands. Yes, really. Some chips ignore everything otherwise. The library does this for you; custom init code must too.

**The screen is white, or shows random coloured snow, and never changes.** It has power but hasn't been started. Same list: wiring, driver, speed.

**`qg_screen_init` returned `QG_ERR_ARG`.** No driver in the config, or a framebuffer backend without a framebuffer big enough for the screen.

**The Pico's LED blinks and nothing else happens (examples only).** That's `board.c` saying a screen failed to start. The USB serial monitor has the message.

---

## Help, my red looks blue

Run the [colour check example](03-examples.md#15-colour-check) and compare it with its pictures. Then find your symptom. The panel settings live in the screen's config (or `examples/board.h`). Each is a yes/no, so if a setting is wrong, the fix is always the other value: coin-flipping with a 100% success rate.

| What you see | What's wrong | The fix |
|---|---|---|
| **Everything looks like a photo negative.** White is black, black is white, red is cyan. The display looks haunted | The panel inverts colours and the library doesn't know (or the other way round) | Flip `invert` |
| **Red and blue have swapped.** The RED box is blue; yellow looks light blue. *Green is fine*, which is the giveaway | The panel wants BGR order, not RGB | Flip `bgr` |
| **Text reads backwards**, or corner labels have swapped left for right | The panel is mirrored | Flip `mirror_x` (and `mirror_y` if top and bottom have swapped) |
| **Everything is sideways or upside down** | Rotation | [`qg_screen_set_rotation`](reference/screens.md#qg_screen_set_rotation), or `.rotation` in the config |
| **A strip of garbage along one or two edges**, or the picture shifted a few pixels | The chip's memory is bigger than its glass, and the picture starts at an offset | Set `x_offset`, `y_offset` in the config |
| **Colours right, but washed out, or shifting as you tilt your head** | The viewing angle. Cheap TN panels look right only from straight on | Look at it from its best angle; buy IPS panels next time |
| **Greys tinted even straight on** (blue-grey, say), pure colours fine, whites cold | The panel's red, green and blue don't brighten at the same rate through the mid-tones | [Calibrate it](03-examples.md#16-calibrate); only that screen changes |
| **Random sparkles or speckles that change** | The SPI signal is struggling | Lower the speed; shorten and reseat the wires |
| **Everything's dim** | Brightness, or the backlight wiring | [`qg_screen_set_brightness`](reference/screens.md#qg_screen_set_brightness); check the backlight pin |
| **Gradients come out in bands** | The standard palette has only 6 levels per colour | Give gradients their own entries with [`qg_palette_set`](reference/colour.md#qg_palette_set); for images on framebuffer screens, [`qg_palette_load_image`](reference/images.md#qg_palette_load_image) |
| **Faint horizontal bands next to bright areas**, fading after a few minutes | Cheap panels do this until they warm up | Nothing; it's the panel, not your data |

**Two things that aren't problems:**
- **Greys read back faintly green** (64, 68, 64): screens store colours as RGB565, with one more bit of green than of red and blue. It's invisible.
- **Photos lie.** Phone cameras rebalance colour, and photos of screens pick up moiré. Judge colour by eye, beside a sheet of white paper; use photos for layout.

---

## Building

**`Cannot specify link libraries for target "my_app" which is not built by this project`.** CMake produces error messages the way a government office produces customer service: technically, and at you. Translation: you declared a program with one name and then configured one with another. Could CMake work out what you meant? Of course it could. Will it? CMake has never guessed anything in its life, and it isn't about to start for you. Make every target name in `CMakeLists.txt` match. See [CMake in five minutes](07-getting-started.md#cmake-in-five-minutes).

**`undefined reference to img_star`** (or to any of your own functions). A `.c` file isn't in the build. Every source file must be listed in `add_executable` (or `target_sources`).

**`undefined reference to qg_asset_init`.** The asset reader is its own library: add `qg4p_assets` to `target_link_libraries`.

**`fatal error: qg4p.h: No such file`.** The program doesn't link `qg4p` (`target_link_libraries(my_app pico_stdlib qg4p)`), or `add_subdirectory(qg4p)` is missing.

**`build.ninja` not found, or the build does nothing.** CMake hasn't been configured (or something changed that needs it again: a new file, a new target). Run *Configure CMake* in VS Code, or `cmake -B build` by hand, then build.

**Debug or Release?** Most of QG4P's speed is set by the SPI wire, so it barely matters: Release was at most 20% faster in measurements. Use Debug while developing.

---

## Loading

**Drag-and-drop didn't take.** Hold BOOTSEL while plugging in; the Pico appears as a drive. On the Pico 2, drag-and-drop can be fussy for some files; `picotool load` is the reliable route.

**Tired of the BOOTSEL shuffle.** Once any QG4P program is running, `picotool load -f -x file.uf2` reboots it into BOOTSEL over USB, loads the new file and starts it. See [switching programs](07-getting-started.md#switching-programs).

**The UF2 is twice the size of the pack.** Normal: each 512-byte UF2 block carries 256 bytes of data.

---

## Drawing and text

**Nothing prints.** The screen has no font in slot 0: [`qg_screen_set_font`](reference/text.md#qg_screen_set_font). Or the text colour is the background colour.

**Everything after one line is dashed** (or thick). The line style (or width) stays set until changed. Set it back: `qg_screen_set_line_style(&scr, 0xFFFF)`, `qg_screen_set_line_width(&scr, 1)`.

**A changing number leaves bits of the old one behind.** A proportional font: a space is narrower than a digit, so `" 42"` doesn't cover `"118"`. Use a monospaced font (`{f:2}` in the examples) for numbers that change, with opaque text.

**Erasing something also erased the thing next to it.** Erasing by redrawing in the background colour erases whatever it crosses. Keep things out of each other's way (see the [dashboard](03-examples.md#13-dashboard)), or use a framebuffer.

**PAINT flooded the whole screen.** There's a gap in the outline, and paint found it. One pixel is enough. With a border colour, it also paints over anything that isn't the border colour, text included.

**The screen went blank at the bottom instead of scrolling.** A DIRECT screen with no text history clears and starts again at the top. Give it history: [scrolling](reference/text.md#scrolling).

**Text scrolled, but the shapes vanished.** DIRECT screens scroll by clearing and reprinting the text they remember; shapes aren't text. A framebuffer screen scrolls everything.

**Text is cut off at the bottom of an area.** A [view](reference/drawing.md#qg_view) is set; inside a view, printing doesn't scroll. `qg_view_reset(&scr)` when done with it.

**Braces vanish from text.** `{` starts markup. Write `{{` for a literal brace.

**`qg_point`, `qg_paint` or `qg_get` does nothing.** They need a framebuffer screen; on a DIRECT screen they return `QG_NONE` or `QG_ERR_UNSUPPORTED`.

**A sprite that restores its background leaves garbage at the edge.** GET must be entirely on the screen; a save-under sprite has to stay fully on screen too (see the [sprites example](03-examples.md#12-sprites)).

---

## Images and assets

**The image doesn't appear.** Check [`qg_image_open`](reference/images.md#qg_image_open)'s result: `QG_ERR_UNSUPPORTED` means not 8-bit, or wider than 480 pixels. Convert it with [`img2bmp8.py`](05-tools.md#img2bmp8py).

**A see-through image has a magenta (or black) box around it.** It was opened without `QG_IMAGE_TRANSPARENT`.

**Image colours look off on a framebuffer screen.** Images there share the screen's one palette. For exact colours: [`qg_palette_load_image`](reference/images.md#qg_palette_load_image).

**`QG_ASSET_ERR_NO_PACK`.** The pack isn't loaded, or went to a different place than `QG_ASSET_PACK_OFFSET` expects.

**`QG_ASSET_ERR_OVERLAP`.** Your program has grown into the pack's space (1 MB). Move the pack up (`--offset` and `QG_ASSET_PACK_OFFSET`, together).

**A text file from the pack prints garbage at the end.** Pack files aren't followed by a `'\0'`. Copy into a buffer and add one.

---

## Speed

**Full-screen updates are slow on a big screen.** The SPI wire's limit: a 320x480 screen is 2.46 million bits a frame, about 65 ms at 37.5 MHz, so roughly 14 full frames a second. Change less of the screen per frame; a framebuffer flush sends only what changed.

**A DIRECT animation flickers.** It shows the moment between erasing and redrawing. Redraw only what changed, keep that moment short, or use a framebuffer (compare examples [8](03-examples.md#8-animation-direct) and [9](03-examples.md#9-framebuffer)).

---

## Memory

**The program ran out of RAM.** Framebuffers are the big item: one byte per pixel (150 KB for 320x480). Then text history (about 126 bytes per line). [`size_report.py`](05-tools.md#size_reportpy) shows exactly where it goes.

**A feature I don't use shows up in the size report.** Something names it. [`size_audit.py`](05-tools.md#size_auditpy) shows which programs include which parts; the usual cause is code that mentions `QG_BACKEND_BUF8` or a driver "just in case".
