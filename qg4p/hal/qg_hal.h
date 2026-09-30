/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg_hal.h
 * @brief   Hardware Abstraction Layer: the shared SPI bus and the devices on it.
 *
 * LAYER:   HAL (bottom of the stack; the only layer that touches pico-sdk
 *          SPI/DMA/GPIO calls)
 * DEPENDS: pico-sdk (hardware_spi, hardware_dma, hardware_gpio, hardware_pwm,
 *          hardware_clocks)
 *
 * ---------------------------------------------------------------------------
 *  THE BIG PICTURE: ONE BUS, SEVERAL SCREENS
 * ---------------------------------------------------------------------------
 *
 *      Pico                           Screen A          Screen B
 *      ----                           --------          --------
 *      SCK  ─────────────────────────── SCK ───────────── SCK      (shared)
 *      MOSI ─────────────────────────── MOSI ──────────── MOSI     (shared)
 *      DC   ─────────────────────────── DC ────────────── DC       (shared)
 *      RST  ─────────────────────────── RST ───────────── RST      (shared)
 *      CS_A ─────────────────────────── CS
 *      CS_B ──────────────────────────────────────────── CS       (one each)
 *
 *  A display ignores everything on SCK/MOSI/DC while its CS ("chip select")
 *  pin is HIGH. So to talk to screen A we pull CS_A low, send bytes, then
 *  pull it high again. Only one CS may ever be low at a time.
 *
 *  This file models that with two structures:
 *    qg_bus_t         - the shared wires + the SPI peripheral + a DMA channel
 *    qg_hal_device_t  - one screen's private bits: its CS pin and SPI speed
 *
 * ---------------------------------------------------------------------------
 *  THE DC (DATA/COMMAND) PIN
 * ---------------------------------------------------------------------------
 *  Display controllers receive two kinds of bytes:
 *    DC = LOW   -> "this byte is a COMMAND"     (e.g. 0x2C = "write memory")
 *    DC = HIGH  -> "this byte is DATA/argument" (e.g. pixel colours)
 *  DC is sampled on the *last bit* of each byte, so it must never change while
 *  a byte is still being shifted out. Every function below waits for the SPI
 *  hardware to be completely idle before touching DC or CS. That wait is the
 *  single most important detail in this file.
 *
 * ---------------------------------------------------------------------------
 *  TRANSACTIONS
 * ---------------------------------------------------------------------------
 *  All traffic happens between qg_hal_begin() and qg_hal_end():
 *
 *      qg_hal_begin(&dev);              // CS low, bus set to dev's speed
 *      qg_hal_write_cmd(&dev, 0x2C);    // command byte
 *      qg_hal_fill_pixels(&dev, c, n);  // n pixels of colour c (DMA)
 *      qg_hal_end(&dev);                // CS high
 */
#ifndef QG_HAL_H
#define QG_HAL_H

#include <stddef.h>
#include "qg_types.h"
#include "qg_config.h"
#include "hardware/spi.h"

/* Forward declaration so the bus can remember which device currently owns it. */
typedef struct qg_hal_device qg_hal_device_t;

/**
 * Wiring description handed to qg_bus_init(). Fill it in once in your app.
 *
 * cs_pins lists the CS pin of EVERY screen on this bus, including ones you
 * have not initialised yet. qg_bus_init() drives them all HIGH first, so an
 * un-initialised screen can never "overhear" traffic meant for another one.
 */
typedef struct {
    spi_inst_t   *spi;       /**< spi0 or spi1 (must match the SCK/MOSI pins).  */
    int8_t        sck_pin;   /**< SPI clock.   Board labels: SCK, SCL, CLK.     */
    int8_t        mosi_pin;  /**< Data to displays. Labels: MOSI, SDA, SDI, DIN.*/
    int8_t        dc_pin;    /**< Data/command select. Labels: DC, RS, A0.      */
    int8_t        rst_pin;   /**< Shared reset, or QG_PIN_NONE. Labels: RST.   */
    const int8_t *cs_pins;   /**< Array of every screen's CS pin on this bus.   */
    uint8_t       cs_count;  /**< Number of entries in cs_pins.                 */
} qg_bus_config_t;

/** Runtime state of one shared SPI bus. Treat as opaque; filled by qg_bus_init(). */
typedef struct {
    spi_inst_t             *spi;
    int8_t                  dc_pin;
    int8_t                  rst_pin;
    int                     dma_chan;    /**< DMA channel claimed for pixel data.  */
    uint32_t                current_hz;  /**< Speed the bus is set to right now.   */
    uint8_t                 data_bits;   /**< 8 (commands) or 16 (pixels).         */
    const qg_hal_device_t *owner;       /**< Device whose CS is low, or NULL.     */
    uint16_t                fill_value;  /**< DMA source for qg_hal_fill_pixels.  */
    bool                    ready;
} qg_bus_t;

/** One screen's connection to the bus. Lives inside qg_screen_t. */
struct qg_hal_device {
    qg_bus_t *bus;
    int8_t     cs_pin;
    uint32_t   hz_requested; /**< What the screen asked for.                    */
    uint32_t   hz_actual;    /**< What the Pico can really generate (<= asked). */
};

/* -------------------------------------------------------------------------- */
/*  Bus setup                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * Set up the shared bus: park every CS high, configure SCK/MOSI/DC, claim a
 * DMA channel, and (if a reset pin is given) pulse the shared reset line.
 *
 * Because RST is shared, this is the ONLY place a hardware reset happens.
 * Each screen's init sequence then uses a *software* reset command, which
 * only affects the screen whose CS is low - so initialising screen B never
 * disturbs an already-running screen A.
 *
 * @return QG_OK, or QG_ERR_ARG for a missing/invalid config.
 */
/**
 * Set up one SPI bus and park every listed CS pin high.
 * @return QG_OK, or QG_ERR_ARG for a missing pin, or SCK/MOSI pins that the
 *         named SPI can't use (GPIO n is SPI (n/8)%2; SCK if n%4 == 2, MOSI
 *         if n%4 == 3).
 */
qg_err_t qg_bus_init(qg_bus_t *bus, const qg_bus_config_t *cfg);

/**
 * Register one screen on the bus and work out the real SPI speed it will get.
 *
 * The Pico makes its SPI clock by dividing a 150 MHz source by an even number
 * (with a second divider after it), so not every speed is possible. Asking for
 * 40 MHz gives 37.5 MHz on a Pico 2. The real value lands in dev->hz_actual.
 */
qg_err_t qg_hal_device_init(qg_hal_device_t *dev, qg_bus_t *bus,
                              int8_t cs_pin, uint32_t hz);

/* -------------------------------------------------------------------------- */
/*  Transactions                                                              */
/* -------------------------------------------------------------------------- */

/** Take the bus: wait for idle, switch to this device's speed, CS low. */
void qg_hal_begin(qg_hal_device_t *dev);

/** Release the bus: wait until the last bit has left, then CS high. */
void qg_hal_end(qg_hal_device_t *dev);

/** Send one command byte (DC low). */
void qg_hal_write_cmd(qg_hal_device_t *dev, uint8_t cmd);

/** Send argument bytes for the previous command (DC high, 8-bit frames). */
void qg_hal_write_data(qg_hal_device_t *dev, const uint8_t *data, size_t len);

/**
 * Send `count` RGB565 pixels from memory using DMA (DC high, 16-bit frames).
 * Blocks until every bit has left the Pico.
 */
void qg_hal_write_pixels(qg_hal_device_t *dev, const uint16_t *pixels, uint32_t count);

/**
 * Send the same RGB565 colour `count` times using DMA. This is how rectangle
 * fills and screen clears are done: the DMA engine re-reads one 16-bit value
 * over and over instead of walking through a buffer, so a full-screen clear
 * needs no RAM at all.
 */
void qg_hal_fill_pixels(qg_hal_device_t *dev, uint16_t rgb565, uint32_t count);

/* -------------------------------------------------------------------------- */
/*  Streaming (overlapped) pixel transfers                                    */
/* -------------------------------------------------------------------------- */
/*
 * For sending a large area as many chunks while preparing the next chunk at
 * the same time:
 *
 *     qg_hal_stream_begin(dev);
 *     fill buffer A;  qg_hal_stream_pixels(dev, A, n);   // A starts sending
 *     fill buffer B;  qg_hal_stream_pixels(dev, B, n);   // waits for A, sends B
 *     fill buffer A;  ...                                 // A is free again
 *     qg_hal_stream_end(dev);                            // waits for the last
 *
 * qg_hal_stream_pixels() returns as soon as its transfer has STARTED, so
 * the buffer passed in must not be touched until the NEXT call returns.
 */
void qg_hal_stream_begin(qg_hal_device_t *dev);
void qg_hal_stream_pixels(qg_hal_device_t *dev, const uint16_t *pixels, uint32_t count);
void qg_hal_stream_end(qg_hal_device_t *dev);

/* -------------------------------------------------------------------------- */
/*  Backlight (PWM)                                                           */
/* -------------------------------------------------------------------------- */

/**
 * Prepare a backlight pin for brightness control, and leave it OFF.
 *
 * HOW PWM BRIGHTNESS WORKS
 * The pin is switched on and off QG_BL_PWM_HZ times per second. The share of
 * each cycle spent "on" (the duty cycle) sets the brightness: 25% on looks
 * about a quarter as bright. Each cycle here is divided into 100 steps, so a
 * brightness percentage maps directly onto a step count.
 *
 * Does nothing if pin is QG_PIN_NONE.
 */
void qg_hal_backlight_init(int8_t pin, bool active_high);

/**
 * Set backlight brightness, 0..100 percent (values above 100 act as 100).
 * active_high: true if HIGH turns the backlight on (true for all three test
 * boards). Does nothing if pin is QG_PIN_NONE.
 */
void qg_hal_backlight_set(int8_t pin, bool active_high, uint8_t percent);

#endif /* QG_HAL_H */
