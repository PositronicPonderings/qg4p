# 2. Quick answers

Find your question; follow the link. (Just want every function on one page? [Too busy to read a manual](quick-reference.md).) Each goes to a reference entry (the exact syntax), an example (a working program), or both.

## Drawing

| How do I... | |
|---|---|
| clear the screen? | [`qg_cls`](reference/drawing.md#qg_cls) |
| draw a line? a thick one? a dashed one? | [`qg_line`](reference/drawing.md#qg_line), [`qg_screen_set_line_width`](reference/drawing.md#qg_screen_set_line_width), [`qg_screen_set_line_style`](reference/drawing.md#qg_screen_set_line_style) |
| draw a box, filled or not? | [`qg_box`](reference/drawing.md#qg_box): pass `QG_TRANSPARENT` for the part you don't want |
| draw a circle, an ellipse, part of one? | [`qg_circle`](reference/drawing.md#qg_circle), [`qg_ellipse`](reference/drawing.md#qg_ellipse), [`qg_arc`](reference/drawing.md#qg_arc) |
| draw a gauge? | [`qg_arc`](reference/drawing.md#qg_arc), and the [dashboard example](03-examples.md#13-dashboard) |
| fill a shape I drew with lines? | [`qg_paint`](reference/drawing.md#qg_paint) (framebuffer screens) |
| find out what colour a pixel is? | [`qg_point`](reference/drawing.md#qg_point) (framebuffer screens) |
| make one layout fit any screen size? | [percentages](reference/percentages.md), and the [layout example](03-examples.md#4-layout) |
| draw inside a panel without working out where it is? | [`qg_view`](reference/drawing.md#qg_view) with `move_origin` |
| keep drawing from spilling out of an area? | [`qg_view`](reference/drawing.md#qg_view) |
| turn the screen sideways? | [`qg_screen_set_rotation`](reference/screens.md#qg_screen_set_rotation) |

## Colour

| How do I... | |
|---|---|
| use a colour that isn't one of the 16 named ones? | [`qg_color_from_rgb`](reference/colour.md#qg_color_from_rgb) |
| get an exact colour? | [`qg_palette_set`](reference/colour.md#qg_palette_set) |
| animate colours without redrawing? | [`qg_palette_set`](reference/colour.md#qg_palette_set) on a framebuffer screen; the [palette effects example](03-examples.md#10-palette-effects) |
| fix a screen whose colours look off? | [troubleshooting](06-troubleshooting.md#help-my-red-looks-blue), then the [calibrate example](03-examples.md#16-calibrate) |

## Text

| How do I... | |
|---|---|
| show text? | [`qg_print_at`](reference/text.md#qg_print_at) anywhere, or [`qg_locate`](reference/text.md#qg_locate) + [`qg_println`](reference/text.md#qg_println) |
| show a number? | format it first: `snprintf(buf, sizeof buf, "%d", n);` then print `buf` |
| change colour or size in the middle of a line? | [markup](reference/text.md#markup): `"{c:RED}red{c:} and {s:2}big"` |
| centre text? right-align it? | [`qg_print_align`](reference/text.md#qg_print_align), or [`qg_print_box`](reference/text.md#qg_print_box) in a column |
| update a number without flicker? | [opaque text](reference/text.md#qg_screen_set_text_bg), in a monospaced font |
| line up columns? | tabs (`\t`, [`qg_screen_set_tab_width`](reference/text.md#qg_screen_set_tab_width)) or `{x:120}` |
| know how big text will be before drawing it? | [`qg_text_measure`](reference/text.md#qg_text_measure) |
| use another font? make my own? | [`QG_FONT_INIT`](reference/text.md#qg_font_init); [`ttf2qg.py`](05-tools.md#ttf2qgpy) |
| make text scroll up like a terminal? | it does, with [text history](reference/text.md#scrolling) on DIRECT screens |

## Images and assets

| How do I... | |
|---|---|
| prepare an image? | [`img2bmp8.py`](05-tools.md#img2bmp8py) |
| show an image? scaled? fitted into a box? | [`qg_image_open`](reference/images.md#qg_image_open), [`qg_image_draw`](reference/images.md#qg_image_draw), [`_scaled`](reference/images.md#qg_image_draw_scaled), [`_fit`](reference/images.md#qg_image_draw_fit) |
| float one image over another? | give the top one a see-through background (`img2bmp8.py` does it from a PNG's transparency) and open it with `QG_IMAGE_TRANSPARENT`: [`qg_image_open`](reference/images.md#qg_image_open) |
| keep images and text out of my program, and change them without rebuilding? | the [asset pack](reference/assets.md): [`mkpack.py`](05-tools.md#mkpackpy), the [asset pack example](03-examples.md#7-asset-pack) |

## Animation

| How do I... | |
|---|---|
| move something without flicker? | a [framebuffer screen](reference/screens.md#framebuffer-screens); compare the [animation examples](03-examples.md#8-animation-direct) |
| move something without a framebuffer? | redraw only what changed: the [animation_direct example](03-examples.md#8-animation-direct) |
| make sprites? | [GET and PUT](reference/blocks.md); the [sprites example](03-examples.md#12-sprites) |

## Screens and hardware

| How do I... | |
|---|---|
| wire a screen? two? | [Getting started](07-getting-started.md#wiring) |
| pick which pins to use? | [choosing pins](07-getting-started.md#choosing-pins) |
| dim the backlight? | [`qg_screen_set_brightness`](reference/screens.md#qg_screen_set_brightness) |
| flash the screen for attention, without the seizure risk of a hard blink? | the [photosensitivity-safer flash](reference/flash.md): [`qg_flash_start`](reference/flash.md#qg_flash_start), then [`qg_flash_update`](reference/flash.md#qg_flash_update) every loop |
| run two screens? | set up one bus and two screens: [screens](reference/screens.md), the [two_screens example](03-examples.md#6-two-screens) |
| use a screen QG4P doesn't support? | [Part 8](08-new-chip.md) |
| find out how much memory my program uses? | [`size_report.py`](05-tools.md#size_reportpy), [`size_audit.py`](05-tools.md#size_auditpy) |
