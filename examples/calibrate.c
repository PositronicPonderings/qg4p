/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    calibrate.c
 * @brief   Example 16: calibrate a panel's colours, live, from the USB serial
 *          monitor. When the greys look grey, it prints a line for board.h.
 *
 * Target: qg4p_calibrate.   Screens: A (or B: see CALIBRATE_B below).
 * Expected: examples/expected/calibrate.png
 *
 * HOW TO USE IT
 *   1. Flash it, then open a serial monitor on the Pico's USB port (in VS
 *      Code: the Serial Monitor tab; 115200 baud, though USB ignores it).
 *   2. Look at the GREY RAMP straight on, in the light you'll use it in,
 *      ideally next to a sheet of white paper. Every step should look grey:
 *      no tint, getting evenly lighter left to right.
 *   3. Press keys in the serial monitor:
 *          r  g  b     choose the red, green or blue channel
 *          +  -        that channel's GAMMA: + darkens its mid-tones
 *          ]  [        that channel's GAIN:  [ dims it at full brightness
 *          space       flip between RAW and ADJUSTED, to compare
 *          0           back to no adjustment
 *          p           print the line for board.h
 *   4. Paste the two printed lines over BOARD_A_ADJUSTED and BOARD_A_ADJUST
 *      (or the _B pair) in board.h. Every example then uses it on that
 *      screen.
 *
 * WHICH KNOB FOR WHICH PROBLEM
 *   Greys tinted (say, blue) in the MIDDLE of the ramp, but white looks
 *   fine: raise that channel's gamma (+) until the middle greys go neutral.
 *   WHITE itself is tinted: lower that channel's gain ([) a few percent.
 *   Usually: gamma first, then a touch of gain.
 *
 * The screen starts from the numbers already in board.h, so you can come
 * back and refine them.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "board.h"

/* 0: calibrate screen A (single-screen setups).  1: calibrate screen B. */
#define CALIBRATE_B  0

#define GREY_FIRST 160          /* palette entries 160..175: a 16-step grey ramp */
#define STEPS      16

static qg_screen_t      *scr;
static qg_color_adjust_t adj;
static qg_color_adjust_state_t adj_state;   /* the tables the library works with */
static bool              raw;           /* showing the unadjusted colours?  */
static int               chan;          /* 0 red, 1 green, 2 blue           */

static void pattern(void)
{
    qg_screen_t *s = scr;
    const int16_t w = qg_screen_width(s), h = qg_screen_height(s);
    static const char *names[3] = { "RED", "GREEN", "BLUE" };
    char buf[64];

    qg_screen_set_color_adjust(s, raw ? NULL : &adj, &adj_state);  /* DIRECT: applies to what we draw next */

    for (int k = 0; k < STEPS; k++) {                   /* exact greys, not the cube's */
        uint8_t v = (uint8_t)(k * 255 / (STEPS - 1));
        qg_palette_set(s, (qg_color_t)(GREY_FIRST + k), v, v, v);
    }

    qg_cls(s, QG_BLACK);
    qg_print_at(s, 4, 2, raw ? "{f:1}{c:LIGHTRED}RAW" : "{f:1}ADJUSTED", QG_DEFAULT, NULL);

    /* The grey ramp: the thing to judge. Big, and on black. */
    const int16_t rt = (int16_t)(h * 12 / 100), rh = (int16_t)(h * 26 / 100);
    const int16_t sw = (int16_t)(w / STEPS);
    for (int k = 0; k < STEPS; k++) {
        qg_box(s, (int16_t)(k * sw), rt, (int16_t)(k * sw + sw - 1), (int16_t)(rt + rh),
               QG_TRANSPARENT, (qg_color_t)(GREY_FIRST + k));
    }

    /* Mid greys as big patches, where tints are easiest to see, and white. */
    const int16_t pt = (int16_t)(rt + rh + h * 3 / 100), ph = (int16_t)(h * 22 / 100);
    const int16_t pw = (int16_t)(w / 4);
    static const int8_t patch[4] = { 4, 8, 11, 15 };    /* ramp steps: 27%, 53%, 73%, white */
    for (int i = 0; i < 4; i++) {
        qg_box(s, (int16_t)(i * pw + 2), pt, (int16_t)(i * pw + pw - 3), (int16_t)(pt + ph),
               QG_TRANSPARENT, (qg_color_t)(GREY_FIRST + patch[i]));
    }

    /* The current numbers, the selected channel highlighted. */
    int16_t y = (int16_t)(pt + ph + h * 4 / 100);
    for (int c = 0; c < 3; c++) {
        snprintf(buf, sizeof buf, "{f:2}%s%-5s gain %3u%%  gamma %u.%02u",
                 c == chan ? "{c:YELLOW}> " : "  ", names[c], adj.gain[c],
                 adj.gamma[c] / 100u, adj.gamma[c] % 100u);
        qg_print_at(s, 4, y, buf, QG_WHITE, NULL);
        y = (int16_t)(y + 16);
    }
    qg_print_at(s, 4, (int16_t)(y + 4), "{f:2}{c:DARKGRAY}keys: r g b  + -  [ ]  spc 0 p", QG_DEFAULT, NULL);
    qg_screen_flush(s);                                  /* in case it's a framebuffer */
}

static void print_line(void)
{
    char which = CALIBRATE_B ? 'B' : 'A';
    printf("\nPaste over these two lines in examples/board.h:\n");
    printf("#define BOARD_%c_ADJUSTED   1\n", which);
    printf("#define BOARD_%c_ADJUST     { { %u, %u, %u }, { %u, %u, %u } }\n\n",
           which, adj.gain[0], adj.gain[1], adj.gain[2],
           adj.gamma[0], adj.gamma[1], adj.gamma[2]);
}

static void help(void)
{
    printf("\n--- QG4P colour calibration (screen %c) ---\n", CALIBRATE_B ? 'B' : 'A');
    printf("  r g b   choose a channel      + -  its gamma (+ = darker mid-tones)\n");
    printf("  ] [     its gain (%%)          space  raw / adjusted\n");
    printf("  0       reset                 p  print the board.h line    h  this help\n");
}

int main(void)
{
#if CALIBRATE_B
    board_init_two();
    scr = &screen_b;
    adj = board_adjust_b;                  /* start from what board.h has */
#else
    board_init();
    scr = &screen_a;
    adj = board_adjust_a;
#endif
    pattern();
    help();

    while (true) {
        int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) { sleep_ms(20); continue; }

        bool redraw = true;
        switch (ch) {
        case 'r': chan = 0; break;
        case 'g': chan = 1; break;
        case 'b': chan = 2; break;
        case '+': case '=': if (adj.gamma[chan] <= 295) adj.gamma[chan] = (uint16_t)(adj.gamma[chan] + 5); break;
        case '-': case '_': if (adj.gamma[chan] >= 55)  adj.gamma[chan] = (uint16_t)(adj.gamma[chan] - 5); break;
        case ']':           if (adj.gain[chan] <= 98)   adj.gain[chan]  = (uint8_t)(adj.gain[chan] + 2);  break;
        case '[':           if (adj.gain[chan] >= 2)    adj.gain[chan]  = (uint8_t)(adj.gain[chan] - 2);  break;
        case ' ': raw = !raw; break;
        case '0': { qg_color_adjust_t none = QG_COLOR_ADJUST_NONE; adj = none; } break;
        case 'p': print_line(); redraw = false; break;
        case 'h': help(); redraw = false; break;
        default:  redraw = false; break;
        }
        if (redraw) {
            pattern();
            printf("%s  R %u%% %u.%02u   G %u%% %u.%02u   B %u%% %u.%02u\n", raw ? "RAW     " : "ADJUSTED",
                   adj.gain[0], adj.gamma[0] / 100u, adj.gamma[0] % 100u,
                   adj.gain[1], adj.gamma[1] / 100u, adj.gamma[1] % 100u,
                   adj.gain[2], adj.gamma[2] / 100u, adj.gamma[2] % 100u);
        }
    }
}
