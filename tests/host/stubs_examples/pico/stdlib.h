/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef unsigned int uint;
void sleep_ms(uint32_t);
uint64_t time_us_64(void);
void tight_loop_contents(void);
bool stdio_init_all(void);
#include "hardware/gpio.h"
/* serial input (pico/stdio.h) */
#define PICO_ERROR_TIMEOUT (-1)
int getchar_timeout_us(uint32_t timeout_us);
