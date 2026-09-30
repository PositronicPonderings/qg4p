# Examples

Sixteen small programs: fourteen from "hello" to a two-screen dice roller, plus two colour tools for when something looks wrong (a test pattern, and a calibrator). Each builds as its own program: one build, sixteen `.uf2` files in `build/examples/`. Flash whichever you like; no CMake editing required.

**First, tell them about your wiring.** Every example gets its screens from `board.h` (with `board_init()`, or `board_init_fb()` for a framebuffer screen). Set your pins and boards there once and all sixteen follow. The defaults match `docs/WIRING.md`. If you want to see how a screen is actually brought to life, read `board.c`: it's short and commented within an inch of its life.

**Switching programs without the BOOTSEL ritual.** Once any QG4P program is running, `picotool` can reboot it into BOOTSEL over USB and load the next one:

```
picotool load -f -x build/examples/qg4p_dice_roller.uf2
```

For a pick list inside VS Code, add the task in `tools/vscode/qg4p_tasks.json` to your `.vscode/tasks.json` (instructions inside the file), then use *Terminal > Run Task... > QG4P: Load a program*.

**Then compare.** Each example has a picture of what it should look like in `expected/`, drawn by the real library code on a PC. Colours on real glass will differ a bit (cheap panels have opinions); shapes, text and positions should match.

| # | Program | Screens | What it shows | Picture |
|---|---|---|---|---|
| 1 | `qg4p_hello` | A | The smallest complete program | [hello](expected/hello.png) |
| 2 | `qg4p_shapes` | A | Every primitive: lines, boxes, circles, ellipses, arcs, points; thickness; line styles | [shapes](expected/shapes.png) |
| 3 | `qg4p_text` | A | Fonts, the print cursor, markup, wrapping, alignment, tabs, a counter that updates in place | [text](expected/text.png) |
| 4 | `qg4p_layout` | A | Percentages: one layout for any size and rotation; a panel drawn inside a VIEW | [upright](expected/layout.png), [sideways](expected/layout_sideways.png) |
| 5 | `qg4p_images` | A | Compiled-in images: 1:1, scaled, fitted, stretched, see-through | [images](expected/images.png) |
| 6 | `qg4p_two_screens` | A + B | One bus, two chips, independent brightness | [A](expected/two_screens_a.png), [B](expected/two_screens_b.png) |
| 7 | `qg4p_asset_pack` | A | Images and text loaded by name from the asset pack | [with pack](expected/asset_pack.png), [without](expected/asset_pack_nopack.png) |
| 8 | `qg4p_animation_direct` | A | Motion without a framebuffer: repair only the patch that changed | [animation_direct](expected/animation_direct.png) |
| 9 | `qg4p_framebuffer` | A (BUF8) | The same motion, flicker-free: two extra lines | [framebuffer](expected/framebuffer.png) |
| 10 | `qg4p_palette_effects` | A (BUF8) | Water and fire that move without redrawing a pixel; exact image colours | [palette_effects](expected/palette_effects.png) |
| 11 | `qg4p_paint` | A (BUF8) | A colouring book: PAINT fills, POINT reads back | [paint](expected/paint.png) |
| 12 | `qg4p_sprites` | A (BUF8) | GET/PUT sprites: stamping, XOR, save-under | [sprites](expected/sprites.png) |
| 13 | `qg4p_dashboard` | A | Gauges, needles, readouts, a bar meter | [dashboard](expected/dashboard.png) |
| 14 | `qg4p_dice_roller` | A + B (BUF8) | The game one: tumbling dice, a roll log, a histogram | [A](expected/dice_roller_a.png), [B](expected/dice_roller_b.png) |
| 15 | `qg4p_colour_check` | A (or both) | A test pattern: named colours with their RGB values, labelled pure colours, ramps, corner labels | [swatches](expected/colour_check.png), [diagnostics](expected/colour_check_diagnostics.png) |
| 16 | `qg4p_calibrate` | A (or B) | Tune a panel's colours live from the USB serial monitor; prints a line for `board.h` | [start](expected/calibrate.png), [after tuning](expected/calibrate_adjusted.png) |

Some pictures are a single moment of something that moves (a bouncing ball, a scrolling log); some include random numbers, which will differ on your screen. That's not a bug. That's dice.

## Lessons hiding in these files

A few things that bit us while writing them, and will bite you too if you let them:

- **Numbers that change need a monospaced font** (`{f:2}`, `font_small`). In a proportional font a space is narrower than a digit, so `" 42"` doesn't fully cover `"118"` and the old number's edges peek out. See `text.c` and `dashboard.c`.
- **A needle erased by redrawing it in black also erases whatever it crossed.** Keep labels out of its path. See `dashboard.c`.
- **Flood fills escape through one-pixel gaps**, and fill *over* anything that isn't the border colour, text included. See `paint.c`.
- **GET must stay on the screen.** A sprite that saves its background has to stay fully visible too. See `sprites.c`.
- **Turn the line style back to solid** (`0xFFFF`) when you're done, or everything after it is dotted too. See `shapes.c`.

## Help, my red looks blue

Run `qg4p_colour_check` and compare it with [its pictures](expected/colour_check_diagnostics.png). Then find your symptom below. (To compare two panels side by side, set `BOTH_SCREENS` to 1 at the top of `colour_check.c`.)

The panel settings live in `examples/board.h` (for your own program, in the screen's `qg_screen_config_t`). Each is a yes/no switch, so if a setting is wrong, the fix is always the other value. No guesswork, just coin-flipping with a 100% success rate.

| What you see | What's wrong | The fix |
|---|---|---|
| **Everything looks like a photo negative.** White is black, black is white, red is cyan. The display looks haunted. | The panel inverts colours and the library doesn't know (or vice versa) | Flip `invert` |
| **Red and blue have swapped.** The RED box is blue; yellow looks light blue; orange looks blue-ish. *Green is fine*, which is the giveaway. | The panel wants its colours in BGR order, not RGB | Flip `bgr` |
| **Text reads backwards**, or the corner labels are in each other's corners left to right | The panel is mirrored | Flip `mirror_x` (and `mirror_y` if top and bottom have swapped) |
| **Everything is sideways or upside down** | Rotation | `qg_screen_set_rotation()`, or `.rotation` in the config |
| **A strip of garbage along one or two edges**, or the image shifted a few pixels | The panel's RAM is bigger than its glass, and the picture starts at an offset | Set the screen's offsets (`x_offset`, `y_offset`); see `docs/WIRING.md` |
| **Colours are right, but washed out, or they shift as you tilt your head** (green goes cyan, dark greys go murky) | The panel's viewing angle. Cheap TN panels look right only from straight on | Look at it from its best angle, or buy IPS panels next time |
| **Greys are tinted even straight on** (blue-grey, say), pure colours fine, and whites look cold | That panel's red, green and blue don't brighten at the same rate through the mid-tones | Calibrate it: run `qg4p_calibrate`, tune until the grey ramp is grey, paste the printed lines into `board.h`. Only that screen changes |
| **Random sparkles, speckles or wrong colours that change each time** | The SPI signal is struggling (long wires, loose jumpers) | Lower the SPI speed (`BOARD_SPI_HZ`), shorten the wires, reseat them |
| **Everything's dim** | Brightness, or the backlight pin | `qg_screen_set_brightness()`; check the backlight wire |
| **Gradients come out in bands** | The standard palette has only 6 levels per channel (and RGB565 32 or 64) | Give smooth gradients their own palette entries with `qg_palette_set()`, as the ramps here do; for images, `qg_palette_load_image()` on framebuffer screens |
| **One panel's colours are simply a bit off** (its green looks cyan next to the other screen) | Panels differ, especially cheap ones | Calibrate it (`qg4p_calibrate`); or, for a few particular colours, tune them on that screen with `qg_palette_set()` |

**Two things that aren't problems:**
- **Greys are faintly green** in the swatch numbers (64, 68, 64): the screens store colours as RGB565, which keeps one more bit of green than of red and blue. The difference is invisible.
- **Photos lie.** Phone cameras rebalance colour, and a photo of a screen picks up moiré patterns. Judge colours by eye; use photos for layout.

### Calibrating a panel

`qg4p_calibrate` is driven from the USB serial monitor (VS Code's *Serial Monitor* tab works). Look at the grey ramp straight on, in the light you'll use the device in, next to a sheet of white paper if you have one. Then:

1. **Tinted mid-greys:** pick the offending channel (`r`, `g` or `b`) and press `+` until the middle of the ramp goes neutral. Gamma bends the mid-tones without moving black or white.
2. **Tinted white:** lower that channel's gain with `[` a couple of steps at a time.
3. Flip between raw and adjusted with **space** to check you've made it better, not just different.
4. Press **`p`**, and paste the two printed lines over `BOARD_A_ADJUSTED` and `BOARD_A_ADJUST` (or the `_B` pair) in `board.h`. Only a screen marked as adjusted carries the colour tables (1.3 KB); the rest pay nothing.

Set `CALIBRATE_B` to 1 at the top of `calibrate.c` to tune screen B. In your own programs, the same numbers go to `qg_screen_set_color_adjust()`, along with a `qg_color_adjust_state_t` you declare for its tables.

## Your own program

Copy an example, rename it, and add one line to `examples/CMakeLists.txt`:

```cmake
qg4p_example(my_thing)              # builds my_thing.c + board.c
qg4p_example(my_other ART)          # ...and links the example images
```

Or leave `examples/` alone and start a project of your own: copy the `qg4p/` folder in, and see the main README.

## The art

`art/make_art.py` draws the images from scratch (so they come with no licence questions) and converts them into `example_art.c`. The asset pack's files are in `pack/`. Build it with:

```
python3 tools/mkpack.py examples/pack --out build/example_assets
```
