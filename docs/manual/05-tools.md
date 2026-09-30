# 5. Tools

Python scripts in [`tools/`](../../tools), run on your PC. They need Python 3; the converters also need Pillow (`pip install pillow`). Run them from the repository's top folder.

[ttf2qg.py](#ttf2qgpy) · [img2bmp8.py](#img2bmp8py) · [mkpack.py](#mkpackpy) · [size_report.py](#size_reportpy) · [size_audit.py](#size_auditpy)

---

## ttf2qg.py

Turns a TrueType or OpenType font into a C file QG4P can print with.

```
python3 tools/ttf2qg.py MyFont.ttf --size 20 --name my_font_20 --out my_font_20.c
```

| Option | Meaning |
|---|---|
| `--size` | the size in pixels (roughly the height of capitals plus descenders) |
| `--name` | the C name of the font data, used in `QG_FONT_INIT(my_font_20, ...)` |
| `--range` | which characters, as code-point ranges: `32-126` (the default: plain ASCII), or e.g. `0x20-0x7E,0xB0` |
| `--chars` | extra characters to include, e.g. `"°±×é"` |
| `--out` | the `.c` file to write |

**Using the result:** add the `.c` file to your build, declare the font, and use it:

```c
extern const lv_font_t my_font_20;
static qg_font_t big = QG_FONT_INIT(my_font_20, QG_DEFAULT, 1);
```

**Notes:** only the characters you include cost flash; the defaults (95 characters) take 1.5 to 4 KB depending on size. Fonts are 1 bit per pixel: crisp, not anti-aliased. The files use LVGL's layout, so fonts from LVGL's own converter also work, if 1 bit per pixel and uncompressed ([`qg_font_check`](reference/text.md#qg_font_check) tells you). Mind the font's licence: many are free to embed, not all.

---

## img2bmp8.py

Turns a PNG, JPG, GIF or BMP into an 8-bit BMP QG4P can draw: as a `.bmp` file (for an asset pack), or also as a C array (to compile in).

```
python3 tools/img2bmp8.py star.png --out star.bmp                       # a file
python3 tools/img2bmp8.py star.png --width 64 --out star.bmp --c-array img_star   # + star.c
```

| Option | Meaning |
|---|---|
| `--out` | the `.bmp` to write |
| `--width`, `--height` | resize; one alone keeps the shape |
| `--colors` | the most colours to use (default 255, or 256 with no transparency) |
| `--dither`, `--no-dither` | smooth gradients by scattering pixels, or don't. **Use `--no-dither` for pixel art and flat colours**; photos usually look better dithered |
| `--key R,G,B` | make this colour see-through, e.g. `255,0,255` for magenta |
| `--uncompressed` | store plain pixels, not RLE8 |
| `--c-array NAME` | also write `NAME.c`, a C array `NAME` and `NAME_size` |

**What it tells you:** the size, how well it compressed, and, for images with see-through parts, "open with `QG_IMAGE_TRANSPARENT`". A PNG's own transparency is kept automatically.

**Notes:** images are compressed with RLE8, which stores runs of identical pixels along each row. It loves flat colours and **horizontal** runs: a gradient running top to bottom (each row one colour) shrinks to almost nothing, while the same gradient running left to right doesn't compress at all. Busy or dithered images can come out bigger compressed than plain, so the script keeps whichever is smaller and says so.

---

## mkpack.py

Bundles a folder of files into an asset pack, ready to load onto the Pico.

```
python3 tools/mkpack.py my_assets --out build/assets
```

Makes `build/assets.uf2` (drag it onto the Pico in BOOTSEL mode) and `build/assets.bin` (for `picotool load build/assets.bin -o 0x10100000`).

- **PNG, JPG and GIF** files are converted to `.bmp` on the way in (so `icons/star.png` becomes `icons/star.bmp`), with transparency detected automatically.
- **Everything else** is stored as it is: text, sounds, game data.
- Names keep their folders, with `/` between them; up to 35 characters. Files starting with `.` are skipped.

| Option | Meaning |
|---|---|
| `--out` | where to write, without the extension |
| `--offset` | where in flash the pack goes (default `0x100000`, 1 MB in). Must match `QG_ASSET_PACK_OFFSET` in your program |
| `--max-size` | the largest pack allowed (default 3 MB, the rest of a 4 MB flash) |
| `--family` | the UF2 family; leave it alone unless drag-and-drop misbehaves |

**pack.txt:** optional, in the folder's top level, one line per file needing special treatment:

```
icons/star.png   no-dither
art/scene.jpg    width=240 dither
ui/logo.bmp      transparent
misc/notes.png   raw
```

Options: `no-dither`, `dither`, `width=N`, `height=N`, `colors=N`, `key=R,G,B`, `transparent`, `opaque`, `raw` (store untouched). Lines starting with `#` are comments.

**Notes:** a UF2 is about twice the size of the pack (each 512-byte UF2 block carries 256 bytes of data); only the pack itself lands in flash. If drag-and-drop ever misbehaves, `picotool` is the reliable route.

---

## size_report.py

Flash and RAM for one program, broken down by source file, from the linker's map file.

```
python3 tools/size_report.py build/examples/qg4p_hello.elf.map
```

Lists each file of `qg4p` and `qg4p_assets`, then totals for your program, the Pico SDK and the C library. Only what's actually linked is counted. See [memory and speed](appendices.md#memory-and-speed) for what's typical.

---

## size_audit.py

Flash and RAM for **every** program in a build, and which library files each includes.

```
python3 tools/size_audit.py build
python3 tools/size_audit.py build --baseline old_sizes.json
```

The second list it prints (library files, and the programs that include them) is for keeping QG4P's "don't pay for what you don't use" promise: a file where you don't expect it (say, `qg_backend_buf8`, the framebuffer, in a program without one) means some code is asking for it without needing it. It saves everything as JSON (`build/size_audit.json`); `--baseline` compares with an earlier one: what grew, what shrank, and which library files came or went.
