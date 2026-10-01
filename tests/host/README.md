# Host tests

The library's drawing, text, image, asset and framebuffer code, run on a PC. Screens are replaced by fake ones that record pixels instead of sending them over SPI; the Pico SDK is replaced by small stand-in headers in `stubs/`.

```
sh tests/host/run_tests.sh
```

Needs gcc, Python 3, Pillow and numpy. Each check prints PASS or FAIL; logs are in `tests/host/build/`.

| Check | What it proves |
|---|---|
| text_units | Kerning (both forms), all four character-map forms, UTF-8 decoding |
| line_widths | Lines of width 1 to 8 are exactly that wide |
| scroll_exact | After 40 lines of scrolling, both screen sizes match an unscrolled reference pixel for pixel (transparent and opaque text) |
| images_vs_pillow | 77 image draws (RLE8 and plain; scaled, stretched, clipped) match Pillow's decoder |
| images_rle_delta | RLE8 delta codes match the BMP specification (checked by hand: Pillow has a bug here) |
| asset_pack | Lookups, alignment, checksum, damage handling |
| buf8_* | Framebuffer screens draw exactly what direct screens do; flushes send only what changed; flood fill; scrolling; images; overlapping sprites |
| flash | The photosensitivity-safer flash, with simulated time: never more than 4 opposing brightness changes (2 flashes) touching any one-second window, even when 10 a second are asked for, with fast, slow and erratic loops, and through a minute of back-to-back cancels and restarts; no step over `QG_FLASH_MAX_STEP`; saturated red refused (checked against all 65,536 RGB565 colours); ends by itself after 5; brightness, pixels and palette restored exactly, on DIRECT and framebuffer screens |
| render_pages | Every page of every hardware test program renders with nothing drawn off-screen |
| render_examples | All 16 examples run unchanged against stand-in screens (`render_example.c`), each stopped at a representative moment (the dice roller also mid-roll, where leftover dice would show), with nothing drawn off-screen |
| manual_examples | Every example in the manual (`docs/manual`) compiles, and runs without drawing off the screen. `python3 tests/host/doc_examples.py` (without `--check`) also refreshes the manual's pictures |
| manual_links | Every link and picture in the documentation points at a file that exists, and every `#anchor` at a real heading |
| ai_disclosure | Every source file carries an `SPDX-AI-Disclosure` tag with a valid level, and `AI_DISCLOSURE.md` exists |
| quick_reference | The one-page quick reference names every public function, and nothing that doesn't exist |
| golden_images | Every rendered page and example screen (79 in all) matches its recorded SHA-256 fingerprint |

**Reference pictures** for people comparing real screens (`tests/hardware/expected/`, `examples/expected/`) are made from these renders: `python3 tests/host/make_expected.py`.

**When a golden image changes on purpose** (you changed a test page or improved drawing), inspect the new image in `build/`, then replace its line in `golden.sha256` with the output of `sha256sum build/<name>.ppm`.
