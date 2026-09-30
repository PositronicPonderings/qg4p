/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
typedef struct { uint32_t csr, div, top; } pwm_config;
pwm_config pwm_get_default_config(void);
void pwm_config_set_clkdiv_int(pwm_config*, uint);
void pwm_config_set_wrap(pwm_config*, uint16_t);
void pwm_init(uint, pwm_config*, bool);
uint pwm_gpio_to_slice_num(uint);
void pwm_set_gpio_level(uint, uint16_t);
