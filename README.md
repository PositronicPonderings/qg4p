# QG4P: QuickGraphics 4 Pico

Graphics for the Raspberry Pi Pico 2 and small SPI screens, in the spirit of QuickBasic: `CLS`, `PSET`, `LINE`, `CIRCLE`, `PAINT`, `LOCATE`, `PRINT`, plus fonts, images, an asset pack and optional flicker-free framebuffers. Small, fast, and commented so heavily it doubles as a textbook.

> **Too busy to read a manual?** [Everything on one page](docs/manual/quick-reference.md).
>
> **The manual is in [`docs/manual`](docs/manual/README.md):** quick answers, 16 examples, a reference entry with a working example and a picture for every function, tools, troubleshooting, getting started, adding a new display chip, and how it all works.

## Repository layout

```
qg4p/            the library: copy this folder into your project
  qg4p.h         the one header a program includes
  assets/        the asset pack reader (optional, separate library)
examples/        16 example programs, each its own build target (wiring: examples/board.h)
tests/
  hardware/      the test programs used to develop the library, one per milestone
  host/          tests that run on a PC, no Pico needed
tools/           font, image and asset-pack converters; size report and audit
docs/            the manual (docs/manual); wiring, sizes, design notes, history
```

## Using it

```cmake
add_subdirectory(qg4p)
target_link_libraries(my_app pico_stdlib qg4p)          # add qg4p_assets for the asset pack
```

```c
#include "qg4p.h"
...
qg_cls(&scr, QG_BLUE);
qg_circle_pct(&scr, 50, 40, 30, QG_WHITE, QG_RED);
qg_print_align(&scr, 250, "Hello, {c:YELLOW}Pico{c:}!", QG_ALIGN_CENTER);
```

## Building this repository

Open the folder with the Raspberry Pi Pico VS Code extension (or configure it with CMake and the Pico SDK). One build produces every program: the examples in `build/examples/` (`qg4p_hello.uf2` and friends) and the hardware tests in `build/tests/hardware/`. Flash whichever you want. Set your wiring once in `examples/board.h`.

## Testing

```
sh tests/host/run_tests.sh
```
Runs the drawing, text, image, asset and framebuffer code on a PC against fake screens, independent references and "golden" fingerprints of 79 rendered screens, including every example. Needs gcc, Python 3, Pillow and numpy.

## Published by

[Positronic Ponderings](https://github.com/PositronicPonderings).

## AI disclosure

See [`AI_DISCLOSURE.md`](AI_DISCLOSURE.md). Every source file also carries its own `SPDX-AI-Disclosure` tag, following the [ai-disclosure convention](https://github.com/ggfevans/ai-disclosure).

## Licence

MIT-0 (see `LICENSE`): do what you like, no credit required. The bundled fonts are converted from DejaVu fonts and keep their own licence (`LICENSE-fonts`).
