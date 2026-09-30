/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_hal.c
 * @brief   Shared-SPI-bus implementation on top of pico-sdk.
 *
 * LAYER:   HAL
 * DEPENDS: pico-sdk hardware_spi, hardware_dma, hardware_gpio, hardware_pwm,
 *          hardware_clocks, pico_time
 *
 * See qg_hal.h for the overview. The comments here focus on the *why* of
 * each hardware step.
 */
#include "hal/qg_hal.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/* ========================================================================== */
/*  Private helpers                                                           */
/* ========================================================================== */

/**
 * Wait until the SPI peripheral has *completely* finished, then tidy up.
 *
 * WHY THIS MATTERS
 * "DMA finished" only means the last value was placed in the SPI transmit
 * FIFO (a small queue). The SPI hardware may still be shifting the last few
 * values out, bit by bit. If we flipped DC or CS now, the display would see
 * the change mid-byte and mis-read it. That produces "random" glitches that
 * are very hard to trace, so we always wait for the BUSY flag to clear.
 *
 * TIDYING UP THE RECEIVE SIDE
 * SPI is full-duplex: every bit sent also clocks one bit *in*. We never read
 * it, so the receive FIFO fills up and the hardware sets an "overrun" flag.
 * That is harmless, but we empty the FIFO and clear the flag so that any later
 * code which *does* read from the bus starts clean.
 */
/**
 * Transfers of this many pixels or fewer are sent by the CPU instead of DMA.
 * Drawing lines and outlines produces many very short runs, and for those the
 * fixed cost of configuring a DMA transfer outweighs the transfer itself.
 */
#define QG_HAL_SMALL_XFER 16u

static void bus_wait_idle(qg_bus_t *bus)
{
    while (spi_is_busy(bus->spi)) {
        tight_loop_contents();
    }
    while (spi_is_readable(bus->spi)) {
        (void)spi_get_hw(bus->spi)->dr;
    }
    spi_get_hw(bus->spi)->icr = SPI_SSPICR_RORIC_BITS; /* clear overrun flag */
}

/**
 * Switch the SPI frame size between 8 bits (commands/arguments) and 16 bits
 * (pixels). Only call when the bus is idle.
 *
 * WHY 16-BIT FRAMES FOR PIXELS?
 * A pixel is a uint16_t in the Pico's memory, stored little-endian (low byte
 * first). The displays want the HIGH byte first. In 16-bit mode the SPI
 * hardware sends each value most-significant-bit first, which is exactly the
 * order the display expects, so no byte-swapping is needed anywhere.
 *
 * SPI MODE 0 (CPOL=0, CPHA=0) is what all three of our controllers expect
 * when a CS pin is used.
 */
static void bus_set_bits(qg_bus_t *bus, uint8_t bits)
{
    if (bus->data_bits == bits) {
        return;
    }
    spi_set_format(bus->spi, bits, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    bus->data_bits = bits;
}

/**
 * Stream `count` 16-bit values to the SPI transmit register using DMA.
 *
 * DMA ("direct memory access") is a small engine that copies data without the
 * CPU. We point it at:
 *   - the source: a pixel buffer (read_increment = true, walk through memory)
 *                 or a single value (read_increment = false, re-read it),
 *   - the destination: the SPI data register (never increments),
 *   - a pacing signal (DREQ) so it only writes when the SPI FIFO has room.
 */
static void bus_dma_send16(qg_bus_t *bus, const void *src, uint32_t count,
                           bool read_increment)
{
    if (count == 0) {
        return;
    }

    dma_channel_config c = dma_channel_get_default_config(bus->dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(bus->spi, true)); /* true = TX side */
    channel_config_set_read_increment(&c, read_increment);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(bus->dma_chan, &c,
                          &spi_get_hw(bus->spi)->dr, /* write to: SPI data reg */
                          src,                       /* read from              */
                          count,                     /* number of transfers    */
                          true);                     /* start immediately      */

    /* v1 is deliberately blocking (see plan section 11). A later version can
     * return here and let the transfer run in the background.               */
    dma_channel_wait_for_finish_blocking(bus->dma_chan);
    bus_wait_idle(bus);
}

/* ========================================================================== */
/*  Bus setup                                                                 */
/* ========================================================================== */

/*
 * Can this GPIO do this job for this SPI? On the RP2040 and RP2350 the rule
 * is regular: GPIO n belongs to SPI (n / 8) % 2, and its job is n % 4:
 *   0 = RX (MISO)   1 = CSn   2 = SCK   3 = TX (MOSI)
 * So GP18 is SPI0's SCK (18 / 8 = 2, even; 18 % 4 = 2), GP11 is SPI1's TX.
 * Getting this wrong gives a black screen and no clue why, so check.
 */
static bool spi_pin_ok(spi_inst_t *spi, int8_t pin, int role)
{
    return pin >= 0 && pin < 48 && (pin % 4) == role &&
           ((pin / 8) % 2) == (int)spi_get_index(spi);
}

qg_err_t qg_bus_init(qg_bus_t *bus, const qg_bus_config_t *cfg)
{
    if (bus == NULL || cfg == NULL || cfg->spi == NULL ||
        cfg->sck_pin < 0 || cfg->mosi_pin < 0 || cfg->dc_pin < 0 ||
        (cfg->cs_count > 0 && cfg->cs_pins == NULL)) {
        return QG_ERR_ARG;
    }
    if (!spi_pin_ok(cfg->spi, cfg->sck_pin, 2) || !spi_pin_ok(cfg->spi, cfg->mosi_pin, 3)) {
        return QG_ERR_ARG;             /* SCK or MOSI can't be used by this SPI */
    }

    bus->spi      = cfg->spi;
    bus->dc_pin   = cfg->dc_pin;
    bus->rst_pin  = cfg->rst_pin;
    bus->owner    = NULL;

    /* 1. Park EVERY chip-select high before the clock line comes alive, so no
     *    screen mistakes start-up noise for a command.                        */
    for (uint8_t i = 0; i < cfg->cs_count; i++) {
        int8_t cs = cfg->cs_pins[i];
        if (cs >= 0) {
            gpio_init((uint)cs);
            gpio_set_dir((uint)cs, GPIO_OUT);
            gpio_put((uint)cs, 1);
        }
    }

    /* 2. DC is an ordinary output. Idle HIGH (= "data").                      */
    gpio_init((uint)cfg->dc_pin);
    gpio_set_dir((uint)cfg->dc_pin, GPIO_OUT);
    gpio_put((uint)cfg->dc_pin, 1);

    /* 3. The SPI peripheral. We start slow and 8-bit; each screen switches to
     *    its own speed in qg_hal_begin().                                    */
    spi_init(bus->spi, QG_BUS_BOOT_HZ);
    bus->current_hz = QG_BUS_BOOT_HZ;
    spi_set_format(bus->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    bus->data_bits = 8;

    /* Hand the SCK and MOSI pins to the SPI peripheral. We do NOT hand over a
     * CS pin: the SPI block's own CS would toggle between every frame, and we
     * need CS held low for a whole transaction. So CS stays a plain GPIO.     */
    gpio_set_function((uint)cfg->sck_pin, GPIO_FUNC_SPI);
    gpio_set_function((uint)cfg->mosi_pin, GPIO_FUNC_SPI);

    /* 4. One DMA channel for all pixel traffic on this bus. `true` means
     *    "panic if none are free", which would be a programming error.        */
    bus->dma_chan = dma_claim_unused_channel(true);

    /* 5. Shared hardware reset. The controllers want: reset LOW for >10 us,
     *    then up to 120 ms before they accept a "sleep out" command.          */
    if (cfg->rst_pin >= 0) {
        gpio_init((uint)cfg->rst_pin);
        gpio_set_dir((uint)cfg->rst_pin, GPIO_OUT);
        gpio_put((uint)cfg->rst_pin, 1);
        sleep_ms(5);
        gpio_put((uint)cfg->rst_pin, 0);
        sleep_ms(10);
        gpio_put((uint)cfg->rst_pin, 1);
        sleep_ms(120);
    }

    bus->ready = true;
    return QG_OK;
}

qg_err_t qg_hal_device_init(qg_hal_device_t *dev, qg_bus_t *bus,
                              int8_t cs_pin, uint32_t hz)
{
    if (dev == NULL || bus == NULL || !bus->ready || cs_pin < 0 || hz == 0) {
        return QG_ERR_ARG;
    }

    dev->bus          = bus;
    dev->cs_pin       = cs_pin;
    dev->hz_requested = hz;

    /* Make sure CS is an output and HIGH, even if it was left out of the
     * bus's cs_pins list.                                                     */
    gpio_init((uint)cs_pin);
    gpio_set_dir((uint)cs_pin, GPIO_OUT);
    gpio_put((uint)cs_pin, 1);

    /* Ask the SDK for this speed once, just to learn what we really get.
     * spi_set_baudrate() returns the achieved rate.                           */
    bus_wait_idle(bus);
    dev->hz_actual  = spi_set_baudrate(bus->spi, hz);
    bus->current_hz = hz;

    return QG_OK;
}

/* ========================================================================== */
/*  Transactions                                                              */
/* ========================================================================== */

void qg_hal_begin(qg_hal_device_t *dev)
{
    qg_bus_t *bus = dev->bus;

    bus_wait_idle(bus);

    /* Different screens can run at different speeds (the ILI9341, for example,
     * is officially slower than the ST7789). Only reprogram the clock when the
     * bus is actually changing hands, since it costs a few microseconds.     */
    if (bus->current_hz != dev->hz_requested) {
        spi_set_baudrate(bus->spi, dev->hz_requested);
        bus->current_hz = dev->hz_requested;
    }

    gpio_put((uint)dev->cs_pin, 0);
    bus->owner = dev;
}

void qg_hal_end(qg_hal_device_t *dev)
{
    qg_bus_t *bus = dev->bus;

    bus_wait_idle(bus);          /* last bit must be out before CS rises */
    gpio_put((uint)dev->cs_pin, 1);
    bus->owner = NULL;
}

void qg_hal_write_cmd(qg_hal_device_t *dev, uint8_t cmd)
{
    qg_bus_t *bus = dev->bus;

    bus_wait_idle(bus);
    bus_set_bits(bus, 8);
    gpio_put((uint)bus->dc_pin, 0);            /* DC low  = command            */
    spi_write_blocking(bus->spi, &cmd, 1);     /* returns once fully shifted   */
    gpio_put((uint)bus->dc_pin, 1);            /* DC back to data (idle state) */
}

void qg_hal_write_data(qg_hal_device_t *dev, const uint8_t *data, size_t len)
{
    qg_bus_t *bus = dev->bus;

    if (len == 0) {
        return;
    }
    bus_wait_idle(bus);
    bus_set_bits(bus, 8);
    gpio_put((uint)bus->dc_pin, 1);
    spi_write_blocking(bus->spi, data, len);
}

void qg_hal_write_pixels(qg_hal_device_t *dev, const uint16_t *pixels, uint32_t count)
{
    qg_bus_t *bus = dev->bus;

    bus_wait_idle(bus);
    bus_set_bits(bus, 16);
    gpio_put((uint)bus->dc_pin, 1);

    if (count <= QG_HAL_SMALL_XFER) {
        /* Short runs: the CPU is quicker than setting up DMA. */
        spi_write16_blocking(bus->spi, pixels, count);
        bus_wait_idle(bus);
        return;
    }
    bus_dma_send16(bus, pixels, count, true);
}

void qg_hal_fill_pixels(qg_hal_device_t *dev, uint16_t rgb565, uint32_t count)
{
    qg_bus_t *bus = dev->bus;

    bus_wait_idle(bus);
    bus_set_bits(bus, 16);
    gpio_put((uint)bus->dc_pin, 1);

    if (count <= QG_HAL_SMALL_XFER) {
        /* Short runs (single pixels, short line segments): feed the SPI
         * transmit queue directly. Configuring a DMA transfer costs a few
         * microseconds, which is more than sending a handful of pixels.   */
        for (uint32_t i = 0; i < count; i++) {
            while (!spi_is_writable(bus->spi)) {
                tight_loop_contents();
            }
            spi_get_hw(bus->spi)->dr = rgb565;
        }
        bus_wait_idle(bus);
        return;
    }

    /* The DMA engine reads from this address repeatedly, so the value must
     * live somewhere that outlasts this call (not a local variable on a stack
     * that might be reused). The bus struct is a safe home.                  */
    bus->fill_value = rgb565;
    bus_dma_send16(bus, &bus->fill_value, count, false);
}

/* ========================================================================== */
/*  Streaming                                                                 */
/* ========================================================================== */

void qg_hal_stream_begin(qg_hal_device_t *dev)
{
    qg_bus_t *bus = dev->bus;
    bus_wait_idle(bus);
    bus_set_bits(bus, 16);
    gpio_put((uint)bus->dc_pin, 1);
}

void qg_hal_stream_pixels(qg_hal_device_t *dev, const uint16_t *pixels, uint32_t count)
{
    qg_bus_t *bus = dev->bus;
    if (count == 0) return;

    /* The previous chunk must finish before the DMA channel is reused. The
     * SPI keeps shifting its last few values out meanwhile; that's fine,
     * because this chunk simply queues behind them in the transmit FIFO.   */
    dma_channel_wait_for_finish_blocking(bus->dma_chan);

    dma_channel_config c = dma_channel_get_default_config(bus->dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(bus->spi, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(bus->dma_chan, &c, &spi_get_hw(bus->spi)->dr,
                          pixels, count, true);        /* start, don't wait */
}

void qg_hal_stream_end(qg_hal_device_t *dev)
{
    qg_bus_t *bus = dev->bus;
    dma_channel_wait_for_finish_blocking(bus->dma_chan);
    bus_wait_idle(bus);
}

/* ========================================================================== */
/*  Backlight (PWM)                                                           */
/* ========================================================================== */

/*
 * PWM SLICES
 * The Pico's PWM hardware is split into "slices". Each slice has one counter
 * and two outputs (A and B), and every GPIO belongs to one slice/output:
 * GP16 is slice 0 A, GP15 is slice 7 B, and so on.
 *
 * Two backlight pins CAN share a slice (e.g. GP14 = 7A and GP15 = 7B). That
 * is fine, because both use the same frequency, but it hides a trap:
 * pwm_init() resets BOTH outputs of the slice to zero. Initialising the
 * second screen's backlight would switch the first one off. So we remember
 * which slices are already set up (one bit per slice) and only initialise
 * each slice once.
 */
static uint32_t s_pwm_slices_ready = 0;

/* Steps per PWM cycle. 100 steps = one step per percent of brightness. */
#define BL_PWM_STEPS 100u

void qg_hal_backlight_init(int8_t pin, bool active_high)
{
    if (pin < 0) {
        return;
    }

    uint slice = pwm_gpio_to_slice_num((uint)pin);

    if ((s_pwm_slices_ready & (1u << slice)) == 0) {
        /* Frequency = system clock / (divider x steps). Solve for the divider
         * using the real system clock (150 MHz on a stock Pico 2), so the
         * frequency stays right even if the clock is ever changed.
         *   150,000,000 / (10,000 Hz x 100 steps) = divider 150            */
        uint32_t div = clock_get_hz(clk_sys) / (QG_BL_PWM_HZ * BL_PWM_STEPS);
        if (div < 1)   div = 1;
        if (div > 255) div = 255;       /* hardware limit for the integer part */

        pwm_config cfg = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&cfg, div);
        /* "wrap" is the last count before the counter restarts: 0..99 is 100 steps */
        pwm_config_set_wrap(&cfg, (uint16_t)(BL_PWM_STEPS - 1u));
        pwm_init(slice, &cfg, true);

        s_pwm_slices_ready |= (1u << slice);
    }

    /* Set the level to "off" BEFORE connecting the pin to the PWM, so the
     * backlight can't flash on during start-up.                             */
    qg_hal_backlight_set(pin, active_high, 0);
    gpio_set_function((uint)pin, GPIO_FUNC_PWM);
}

void qg_hal_backlight_set(int8_t pin, bool active_high, uint8_t percent)
{
    if (pin < 0) {
        return;
    }
    if (percent > 100) {
        percent = 100;
    }

    /* The PWM output is HIGH while the counter is below `level`. So:
     *   level 0   -> always LOW        level 100 -> always HIGH (never drops,
     *                                   because the counter only reaches 99)
     * For an active-low backlight, "bright" means mostly LOW, so we flip it. */
    uint16_t level = active_high ? percent : (uint16_t)(100u - percent);
    pwm_set_gpio_level((uint)pin, level);
}
