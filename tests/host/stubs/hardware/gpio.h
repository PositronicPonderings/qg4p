/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
#define GPIO_OUT 1
#define GPIO_FUNC_SPI 1
#define GPIO_FUNC_PWM 4
void gpio_init(uint); void gpio_set_dir(uint,bool); void gpio_put(uint,bool); void gpio_set_function(uint,int);
