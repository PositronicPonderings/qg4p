# Flash (photosensitivity-safer)

[start](#qg_flash_start) · [the config](#the-flash-config) · [update](#qg_flash_update) · [cancel](#qg_flash_cancel) · [active?](#qg_flash_active) · [red check](#qg_flash_is_saturated_red) · [the limits](#the-limits-and-why)

Header: `qg_flash.h`. It is **not** included by `qg4p.h`: include it yourself. Its code is linked only if you call it.

Sometimes a game needs the player to look **now**: a natural 20, a critical hit, a reactor about to go bang. Flashing the screen does the job. It can also trigger seizures in people with photosensitive epilepsy, so this flash is built to stay inside the W3C's guidance on flashing, [WCAG 2.3.1 "Three Flashes or Below Threshold"](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold), with a margin. You don't have to think about any of that: the easy way to flash is also the safer way.

**Safer, not safe.** No software can promise that a flash won't trigger a seizure in a particular person. Give your device a setting that turns flashing off, and when it's off, simply don't call `qg_flash_start`.

**How it works:** it gently dims the screen's backlight and brings it back, with no hard on/off steps. It never touches a pixel, so it works the same on DIRECT and framebuffer screens, costs nothing per pixel, and puts the screen back exactly as it was. The screen needs a backlight pin (`bl_pin`). Your program keeps running: you start the flash, then call `qg_flash_update` every time round your main loop.

---

## qg_flash_start

Starts a flash. Returns at once: the flash happens as you call [`qg_flash_update`](#qg_flash_update).

```c
qg_err_t qg_flash_start(qg_flash_t *f, qg_screen_t *scr,
                        const qg_flash_config_t *cfg, uint32_t now_ms);
```

```c example=flash_start compile-only
static qg_flash_t crit;                   /* yours: static, one per screen  */
bool flashing_allowed = true;             /* a setting the player can turn off */

qg_print_align(&scr, 140, "{f:1}CRITICAL!", QG_ALIGN_CENTER);
if (flashing_allowed) {
    qg_flash_start(&crit, &scr, NULL, (uint32_t)(time_us_64() / 1000));
}
```

`NULL` asks for the defaults: three flashes, one a second, dimming to half brightness. `now_ms` is your clock in milliseconds; on a Pico, `to_ms_since_boot(get_absolute_time())` (or `time_us_64() / 1000`).

**Returns:** `QG_OK`, also when there's nothing to do (the backlight is off or at 0 %, or this `f` is already flashing, in which case it carries on rather than starting again). `QG_ERR_ARG` for a `NULL`, or if the screen's foreground or background colour, or the config's `.color`, is a saturated red (see [below](#qg_flash_is_saturated_red)). `QG_ERR_UNSUPPORTED` if the screen has no backlight pin.

**Notes:** keep `f` static or global, start it zeroed (static variables are), and reuse the same one for the same screen: it remembers its last flash, which is how the rate cap holds when you start again straight away. Two screens: one `qg_flash_t` each, started with the same `now_ms`, so they dip together. Don't change the brightness yourself during a flash; if you do, the flash stops and leaves your brightness alone.
**See also:** [`qg_screen_set_brightness`](screens.md#qg_screen_set_brightness)

### The flash config

```c
typedef struct {
    uint16_t   period_ms;  /* time per flash: 0 = 1000; clamped to 800..4000  */
    uint8_t    count;      /* flashes: 0 = 3; clamped to 1..5                  */
    uint8_t    depth;      /* how far to dim, % of the current brightness:
                              0 = 50; 100 = all the way to dark               */
    qg_color_t color;      /* the colour you're drawing attention with, to be
                              checked for red too; QG_DEFAULT = none          */
} qg_flash_config_t;
```

```c example=flash_config compile-only
static qg_flash_t alarm;
qg_flash_config_t cfg = { .period_ms = 100,      /* asks for 10 a second... */
                          .count = 20,           /* ...twenty times...      */
                          .depth = 70,
                          .color = QG_YELLOW };  /* the banner's colour     */
qg_flash_start(&alarm, &scr, &cfg, (uint32_t)(time_us_64() / 1000));
/* ...and gets five flashes at the safer rate, dimming to 30 %. No error. */
```

Every field is **clamped, never refused**: a request for something faster, longer or more often than the limits gets the limits.

---

## qg_flash_update

Moves the flash along. Call it every time round your main loop.

```c
bool qg_flash_update(qg_flash_t *f, uint32_t now_ms);
```

```c example=flash_update compile-only
static qg_flash_t crit;
qg_flash_start(&crit, &scr, NULL, (uint32_t)(time_us_64() / 1000));
for (int frame = 0; frame < 300; frame++) {          /* your game loop */
    uint32_t now = (uint32_t)(time_us_64() / 1000);
    qg_flash_update(&crit, now);                     /* a few microseconds */
    /* ... move things, draw, flush: the game keeps running ... */
    sleep_ms(10);
}
```

**Returns:** `true` while the flash is running; `false` once it's over and the brightness is back exactly where it started.

**Notes:** any loop speed works; 50 times a second or more looks smoothest. A slow loop makes the flash run in slow motion rather than jump: the flash's clock moves at most 20 ms per call. Calling it when nothing is flashing just returns `false`.

---

## qg_flash_cancel

Stops a flash early, and puts the brightness back.

```c
void qg_flash_cancel(qg_flash_t *f, uint32_t now_ms, bool at_once);
```

```c example=flash_cancel compile-only
static qg_flash_t crit;
uint32_t now = (uint32_t)(time_us_64() / 1000);
qg_flash_start(&crit, &scr, NULL, now);
/* ... the player pressed a button: enough flashing ... */
qg_flash_cancel(&crit, now, false);           /* ease back up, smoothly */
while (qg_flash_update(&crit, (uint32_t)(time_us_64() / 1000))) {
    sleep_ms(10);                             /* at most one ramp: under a second at the defaults */
}
```

**Notes:** `at_once = false` turns the dip round and eases back up along the same curve; keep calling `qg_flash_update` until it returns `false`. `at_once = true` puts the brightness back immediately, in one step: for when the screen is about to be switched off or the device is going to sleep. Prefer `false`. Harmless to call when nothing is flashing.

---

## qg_flash_active

Is a flash running?

```c
bool qg_flash_active(const qg_flash_t *f);
```

```c example=flash_active compile-only
static qg_flash_t crit;
if (!qg_flash_active(&crit)) {
    qg_print_at(&scr, 10, 10, "Calm", QG_DEFAULT, NULL);
}
```

**Notes:** the same answer the last `qg_flash_update` gave.

---

## qg_flash_is_saturated_red

Would this colour count as a "saturated red" for flashing?

```c
bool qg_flash_is_saturated_red(const qg_screen_t *scr, qg_color_t color);
```

```c example=flash_red compile-only
qg_color_t banner = QG_RED;
if (qg_flash_is_saturated_red(&scr, banner)) {
    banner = QG_YELLOW;                       /* louder anyway */
}
```

**Notes:** WCAG's working definition: R / (R + G + B) is 0.8 or more, using the colour as the screen's palette holds it. `QG_RED` (170, 0, 0) is; `QG_LIGHTRED` (255, 85, 85) is not (0.6), nor is `QG_MAGENTA`. Red flashes get their own, stricter rule in the guidance, so `qg_flash_start` refuses (`QG_ERR_ARG`) rather than flash a screen whose foreground, background or highlight colour is a saturated red. It refuses rather than swapping colours because it never draws anything: the colours on the screen are yours to choose. It checks those three colours, not every pixel (that would cost what the backlight saves), so keep big areas of saturated red off a screen you flash.

---

## The limits, and why

| Limit | Value | Why |
|---|---|---|
| Flashes in any one second | at most 2 | WCAG 2.3.1 allows 3; we keep a margin. Enforced by one rule: a dip can't begin until 1000 ms after the dip two before it ended, so no second can hold more than four changes (two flashes). It holds however you cancel and restart, as long as you reuse the same `qg_flash_t`. |
| Period | 800 to 4000 ms | 800 is the fastest a steady flash can go while obeying the rule above on its own. |
| Flashes per start | at most 5 | Nothing flashes indefinitely. |
| Biggest step | 12 percentage points per update | Smooth: a raised-cosine curve, and a clock that moves at most 20 ms per update. |
| Depth | 50 % by default | Half brightness is plenty to notice, and a smaller swing is gentler. |
| Saturated red | refused | WCAG treats red flashes as a hazard of their own. |

WCAG's other escape, flashes covering only a small part of the view, doesn't apply: a 2 to 3.5 inch screen at arm's length *is* the small part of the view. So the rate is the guarantee. Every limit is proved by `tests/host/test_flash.c`, which drives the flash with simulated time (fast, slow and erratic loops; requests for 10 flashes a second; cancels and restarts back to back) and checks every one-second window.
