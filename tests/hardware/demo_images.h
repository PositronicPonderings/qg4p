/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    demo_images.h
 * @brief   The M6 test images, compiled in from demo_images.c.
 *
 * demo_images.c is generated: see images/convert_test_images.sh. Each image
 * is a whole BMP file as a byte array, plus its size.
 */
#ifndef DEMO_IMAGES_H
#define DEMO_IMAGES_H
#include <stdint.h>

extern const uint8_t  img_d20[];        /* 64x64, transparent   */
extern const uint32_t img_d20_size;
extern const uint8_t  img_potion[];     /* 16x16, transparent   */
extern const uint32_t img_potion_size;
extern const uint8_t  img_banner[];     /* 240x48               */
extern const uint32_t img_banner_size;
extern const uint8_t  img_landscape[];  /* 240x160              */
extern const uint32_t img_landscape_size;

#endif /* DEMO_IMAGES_H */
