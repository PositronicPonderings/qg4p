/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_driver.c
 * @brief   Driver code shared by every MIPI-DCS controller (ST7789, ILI9341,
 *          ST7796): run an init table, build MADCTL, set the drawing window.
 *
 * LAYER:   Drivers
 * DEPENDS: hal/qg_hal.h, pico_time (sleep_ms)
 */
#include "drivers/qg_driver.h"
#include "pico/stdlib.h"

void qg_driver_run_init(qg_hal_device_t *dev, const qg_driver_t *drv)
{
    for (uint8_t i = 0; i < drv->init_count; i++) {
        const qg_init_cmd_t *step = &drv->init_seq[i];

        /* Each step is its own transaction: CS goes low, the command and its
         * arguments are sent, then CS goes HIGH again before any pause.
         *
         * WHY NOT HOLD CS LOW FOR THE WHOLE SEQUENCE?  The controllers use the
         * rising edge of CS to resynchronise their serial receiver. If a step
         * resets the chip (or it glitches) while CS stays low, every later
         * byte can be misread, which gives a black screen with the backlight
         * on. Releasing CS per step costs microseconds and removes that risk.
         * (Found the hard way during bring-up: holding CS low through a     *
         * software reset left the ST7789 with a black screen.)              */
        qg_hal_begin(dev);
        qg_hal_write_cmd(dev, step->cmd);
        qg_hal_write_data(dev, step->data, step->len);
        qg_hal_end(dev);

        /* The datasheets demand pauses after some commands, e.g. 120 ms after
         * "sleep out" while the chip's internal voltage pumps stabilise.
         * Skipping them is a classic cause of screens that work *sometimes*. */
        if (step->delay_ms) {
            sleep_ms(step->delay_ms);
        }
    }
}

uint8_t qg_driver_madctl(qg_rotation_t rot, bool mirror_x, bool mirror_y, bool bgr)
{
    /*
     * HOW ROTATION WORKS IN HARDWARE
     * The chip can reverse its column order (MX), its row order (MY) and
     * swap rows with columns (MV). Combining them gives the four rotations:
     *
     *    0 deg : nothing
     *   90 deg : swap + mirror columns   (MV | MX)
     *  180 deg : mirror both             (MX | MY)   (a 180 turn = two flips)
     *  270 deg : swap + mirror rows      (MV | MY)
     *
     * Rotating this way costs nothing at draw time: the chip itself remaps
     * every pixel, so the library just draws in the new width and height.
     */
    static const uint8_t rot_bits[4] = {
        0x00,
        QG_MADCTL_MV | QG_MADCTL_MX,
        QG_MADCTL_MX | QG_MADCTL_MY,
        QG_MADCTL_MV | QG_MADCTL_MY,
    };

    uint8_t m = rot_bits[(unsigned)rot & 3u];

    /* Panel quirks are applied with XOR (^) rather than OR (|). If a panel is
     * glued on mirrored, *every* rotation must flip that axis once more, so
     * we toggle the bit instead of just setting it.                          */
    if (mirror_x) m ^= QG_MADCTL_MX;
    if (mirror_y) m ^= QG_MADCTL_MY;
    if (bgr)      m |= QG_MADCTL_BGR;

    return m;
}

void qg_driver_set_window(qg_hal_device_t *dev,
                           uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* Addresses are sent as 16-bit big-endian numbers: high byte first. */
    uint8_t buf[4];

    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)x0;
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)x1;
    qg_hal_write_cmd(dev, QG_CMD_CASET);
    qg_hal_write_data(dev, buf, 4);

    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)y0;
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)y1;
    qg_hal_write_cmd(dev, QG_CMD_RASET);
    qg_hal_write_data(dev, buf, 4);

    qg_hal_write_cmd(dev, QG_CMD_RAMWR);
    /* The caller now streams exactly (x1-x0+1)*(y1-y0+1) pixels. */
}
