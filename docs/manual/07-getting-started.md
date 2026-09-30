# 7. Getting started

From a bag of parts to a circle on a screen. You'll need a Raspberry Pi Pico 2, one or two supported screens ([About](01-about.md)), jumper wires and a breadboard, and the Pico SDK set up on your computer. For the SDK, the **Raspberry Pi Pico extension for VS Code** is the easy way; Raspberry Pi's own documentation covers installing it better than we could.

[Wiring](#wiring) · [Physical pins and GP numbers](#physical-pins-and-gp-numbers) · [Choosing pins](#choosing-pins) · [Layouts for two screens](#layouts-for-two-screens) · [Building](#building) · [CMake in five minutes](#cmake-in-five-minutes) · [The first program](#the-first-program) · [Switching programs](#switching-programs)

---

## Wiring

A screen board has a row of labelled pins. Unfortunately, every seller labels them differently. Here's what they mean:

| Job | Labels you'll see | Goes to | Notes |
|---|---|---|---|
| Ground | GND | any Pico GND | shared by everything |
| Power | VCC, VIN | 3V3 (pin 36) | many boards also accept 5 V (VSYS, pin 39); check yours |
| SPI clock | SCK, SCL, CLK | an SPI **SCK** pin | shared by all screens |
| SPI data | MOSI, SDA, SDI, DIN | an SPI **TX** pin | shared by all screens |
| Data/command | DC, RS, A0 | any GPIO | shared |
| Reset | RST, RES | any GPIO | shared; optional |
| Chip select | CS, SS | any GPIO | **one per screen** |
| Backlight | BL, LED, BLK | any GPIO | one per screen; optional, but needed for dimming |
| Data from the screen | MISO, SDO | nothing | QG4P never reads from screens |

"SCL" and "SDA" are I2C names, but on these boards they mean SPI clock and data. Don't ask. The boards just do that.

Some boards also have touch or SD-card pins (T_CLK, SD_CS and so on); leave them unconnected unless you're using them.

Wire colours are your choice, but a consistent scheme saves real grief when you're counting wires at midnight. One that works: SCK yellow, MOSI orange, CS green, DC blue, RST white, backlight purple, VCC red, GND black. The tested boards' exact settings and wiring are in [`docs/WIRING.md`](../WIRING.md).

---

## Physical pins and GP numbers

The Pico has **two** numbering schemes, and mixing them up is the number one wiring mistake:
- **Physical pin numbers**, 1 to 40, count around the board: 1 to 20 down the left side (USB at the top), 21 to 40 up the right.
- **GP numbers** (GPIO numbers) are what code uses: `.sck_pin = 18` means GP18, which is physical pin 24.

Ground pins sit between the GPIOs, which is why counting along the header drifts by one.

| Left side | | | Right side | |
|---|---|---|---|---|
| **Pin** | **Is** | | **Pin** | **Is** |
| 1 | GP0 | | 40 | VBUS (USB 5 V) |
| 2 | GP1 | | 39 | VSYS |
| 3 | GND | | 38 | GND |
| 4 | GP2 | | 37 | 3V3_EN |
| 5 | GP3 | | 36 | 3V3 (out) |
| 6 | GP4 | | 35 | ADC_VREF |
| 7 | GP5 | | 34 | GP28 |
| 8 | GND | | 33 | GND |
| 9 | GP6 | | 32 | GP27 |
| 10 | GP7 | | 31 | GP26 |
| 11 | GP8 | | 30 | RUN |
| 12 | GP9 | | 29 | GP22 |
| 13 | GND | | 28 | GND |
| 14 | GP10 | | 27 | GP21 |
| 15 | GP11 | | 26 | GP20 |
| 16 | GP12 | | 25 | GP19 |
| 17 | GP13 | | 24 | GP18 |
| 18 | GND | | 23 | GND |
| 19 | GP14 | | 22 | GP17 |
| 20 | GP15 | | 21 | GP16 |

Raspberry Pi's official pinout diagram is in the [Pico 2 datasheet](https://datasheets.raspberrypi.com/pico/pico-2-datasheet.pdf); it's worth printing and keeping on the bench.

---

## Choosing pins

Only **two** of a screen's wires are fussy: SCK and MOSI must be pins that the SPI you choose can use. Everything else (CS, DC, RST, backlight) can go on **any** GPIO, so put those wherever they're convenient.

**The rule for SCK and MOSI**, which holds for every GPIO on the Pico and Pico 2:

> GPIO *n* belongs to SPI **(n ÷ 8) mod 2** (0 or 1, rounding down the division), and its job is **n mod 4**: 0 = MISO (RX), 1 = CS, **2 = SCK**, **3 = MOSI (TX)**.

So GP18: 18 ÷ 8 = 2, which is even, so SPI0; 18 mod 4 = 2, so SCK. GP11: 11 ÷ 8 = 1, odd, so SPI1; 11 mod 4 = 3, so MOSI.

| | SCK can be | MOSI can be |
|---|---|---|
| **spi0** | GP2, GP6, GP18, GP22 | GP3, GP7, GP19 |
| **spi1** | GP10, GP14, GP26 | GP11, GP15, GP27 |

Pick an SCK and a MOSI from the **same row**, and give that row's name to `.spi` in the bus config. Get it wrong and [`qg_bus_init`](reference/screens.md#qg_bus_init) returns `QG_ERR_ARG`, which beats the black screen you'd otherwise get.

**Backlights and brightness:** any GPIO can dim a backlight (they all have PWM). Two backlights on neighbouring pins (like GP16 and GP17) share one PWM unit but still dim independently.

**Leave free what you'll need later:** GP26 to GP28 are the only pins that can read analogue voltages (a knob, a battery level). If you'll ever want those, wire screens elsewhere.

---

## Layouts for two screens

Three ways to wire two screens, all tested against the rule above. Choose by where the rest of your project's wiring goes.

**A. All on the left side** (SPI0, low pins): compact, leaves the right side free.

| Wire | GP | Pin | | Wire | GP | Pin |
|---|---|---|---|---|---|---|
| SCK | GP2 | 4 | | CS screen A | GP6 | 9 |
| MOSI | GP3 | 5 | | CS screen B | GP7 | 10 |
| DC | GP4 | 6 | | Backlight A | GP8 | 11 |
| RST | GP5 | 7 | | Backlight B | GP9 | 12 |

**B. All on the right side** (SPI0, high pins): the layout this library was developed on, with screen B's backlight moved to the right side too.

| Wire | GP | Pin | | Wire | GP | Pin |
|---|---|---|---|---|---|---|
| SCK | GP18 | 24 | | CS screen A | GP17 | 22 |
| MOSI | GP19 | 25 | | CS screen B | GP22 | 29 |
| DC | GP20 | 26 | | Backlight A | GP16 | 21 |
| RST | GP21 | 27 | | Backlight B | GP26 | 31 |

**C. Two separate buses** (SPI0 and SPI1): each screen has its own wires, so each keeps its own speed without the bus switching between them. More wires; worth it if the screens are far apart, or one is much slower than the other.

| Screen A (spi0) | GP | Pin | | Screen B (spi1) | GP | Pin |
|---|---|---|---|---|---|---|
| SCK | GP2 | 4 | | SCK | GP10 | 14 |
| MOSI | GP3 | 5 | | MOSI | GP11 | 15 |
| DC | GP4 | 6 | | DC | GP12 | 16 |
| RST | GP5 | 7 | | RST | GP13 | 17 |
| CS | GP6 | 9 | | CS | GP14 | 19 |
| Backlight | GP7 | 10 | | Backlight | GP15 | 20 |

For layout C, set up two buses (two `qg_bus_t`, one with `.spi = spi0`, one with `.spi = spi1`), each listing its own screen's CS, and give each screen its own bus in `qg_screen_init`.

Whatever you choose, put the numbers in one place (the examples use `examples/board.h`), and every program follows.

---

## Building

This repository builds everything at once: the library, 16 examples and the hardware test programs.

1. Open the repository's folder in VS Code with the Pico extension. Since this project wasn't created by the extension, use its **Import Project** command; it may add its own settings block at the top of `CMakeLists.txt`, which is fine.
2. **Configure CMake**, then **Compile Project**. The programs appear as `.uf2` files: `build/examples/qg4p_hello.uf2` and friends, and `build/tests/hardware/qg4p_m0.uf2` onwards.
3. Load one: hold BOOTSEL, plug the Pico in, drag the `.uf2` onto the drive that appears.

To use QG4P in **your own** project, copy the `qg4p/` folder into it and see the next section.

---

## CMake in five minutes

CMake reads `CMakeLists.txt` and works out how to build your program. A Pico project with QG4P needs about ten lines, and every one of them matters:

```cmake
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)          # find the Pico SDK (the file comes with it)
project(my_project C CXX ASM)           # a name for the whole project
pico_sdk_init()                         # set up the SDK

add_subdirectory(qg4p)                  # build the library (the folder you copied in)

add_executable(my_app main.c)           # your program: its name, and EVERY .c file in it
target_link_libraries(my_app            # what it uses:
    pico_stdlib                         #   the Pico basics
    qg4p)                               #   QG4P (add qg4p_assets for the asset pack)
pico_enable_stdio_usb(my_app 1)         # printf() goes to USB serial
pico_add_extra_outputs(my_app)          # make my_app.uf2 as well
```

The things that go wrong, and why:
- **The program's name must be the same everywhere** it appears: `add_executable(my_app ...)`, `target_link_libraries(my_app ...)`, and so on. A mismatch gives "not built by this project", CMake's way of saying you've introduced it to a stranger.
- **Every `.c` file must be listed** in `add_executable`. A missing one gives "undefined reference" when linking, naming something from that file.
- **After adding a file or a target, configure again** (*Configure CMake*). CMake only reads `CMakeLists.txt` when configuring.
- Several programs in one project are just several `add_executable` blocks, each with its own name. That's how this repository builds 16 examples at once (`examples/CMakeLists.txt` wraps it in a small function).

**Changing a setting** from `qg_config.h`: put it on the library, marked `PUBLIC`, so the library and your program agree on its value:

```cmake
target_compile_definitions(qg4p PUBLIC QG_TEXT_CELL_PIXELS=0)
```

---

## The first program

With your wiring in [`examples/board.h`](../../examples/board.h), `qg4p_hello` is the first thing to try. Here is the same program without the examples' helper, the whole setup in view; the parts in `board.h` are the same numbers.

```c
#include "pico/stdlib.h"
#include "qg4p.h"

static qg_bus_t    bus;
static qg_screen_t scr;
static qg_font_t   body = QG_FONT_INIT(qg_font_sans_16, QG_DEFAULT, 1);

int main(void)
{
    /* 1. The bus: shared wires, and every screen's CS. */
    static const int8_t cs_pins[] = { 17 };
    const qg_bus_config_t bus_cfg = {
        .spi = spi0, .sck_pin = 18, .mosi_pin = 19, .dc_pin = 20, .rst_pin = 21,
        .cs_pins = cs_pins, .cs_count = 1,
    };
    if (qg_bus_init(&bus, &bus_cfg) != QG_OK) while (true) tight_loop_contents();

    /* 2. The screen: chip, pins, and the panel's quirks. */
    const qg_screen_config_t scr_cfg = {
        .driver = QG_DRIVER_ST7789, .cs_pin = 17, .bl_pin = 16, .bl_active_high = true,
        .spi_hz = 40000000u, .width = 240, .height = 320, .invert = true,
    };
    if (qg_screen_init(&scr, &bus, &scr_cfg) != QG_OK) while (true) tight_loop_contents();

    /* 3. Draw. */
    qg_screen_set_font(&scr, 0, &body);
    qg_cls(&scr, QG_BLUE);
    qg_circle_pct(&scr, 50, 40, 30, QG_WHITE, QG_RED);
    qg_print_align(&scr, 250, "Hello, {c:YELLOW}Pico{c:}!", QG_ALIGN_CENTER);

    while (true) tight_loop_contents();
}
```

If it's black, or the colours are wrong, [troubleshooting](06-troubleshooting.md) has you covered. If it works: everything from here on is just drawing.

---

## Switching programs

Holding BOOTSEL and replugging for every test gets old fast. Once **any** QG4P program is running (they all enable USB serial, which includes a reset interface), `picotool` can reboot it into BOOTSEL over USB:

```
picotool load -f -x build/examples/qg4p_dashboard.uf2
```

`-f` forces the reboot; `-x` starts the new program. The button is only needed when the Pico is running something without USB serial, or is new.

**In VS Code:** [`tools/vscode/qg4p_tasks.json`](../../tools/vscode/qg4p_tasks.json) is a task that shows a list of every program, compiles, and loads your pick that way. Copy it into your `.vscode/tasks.json` (the instructions are inside), then use *Terminal > Run Task... > QG4P: Load a program*.
