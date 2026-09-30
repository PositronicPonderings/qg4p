/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef unsigned int uint;
typedef struct { volatile uint32_t dr, sr, icr; } spi_hw_t;
typedef struct spi_inst spi_inst_t;
extern spi_inst_t *spi0;
#define SPI_SSPICR_RORIC_BITS 1u
typedef enum {SPI_CPOL_0} spi_cpol_t; typedef enum {SPI_CPHA_0} spi_cpha_t; typedef enum {SPI_MSB_FIRST} spi_order_t;
uint spi_init(spi_inst_t*, uint); uint spi_set_baudrate(spi_inst_t*, uint);
void spi_set_format(spi_inst_t*, uint, spi_cpol_t, spi_cpha_t, spi_order_t);
bool spi_is_busy(const spi_inst_t*); bool spi_is_readable(const spi_inst_t*);
spi_hw_t *spi_get_hw(spi_inst_t*); uint spi_get_dreq(spi_inst_t*, bool);
int spi_write_blocking(spi_inst_t*, const uint8_t*, size_t);
bool spi_is_writable(const spi_inst_t*);
int spi_write16_blocking(spi_inst_t*, const uint16_t*, size_t);
static inline unsigned spi_get_index(spi_inst_t *spi) { (void)spi; return 0; }
