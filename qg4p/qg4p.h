/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qg4p.h
 * @brief   QG4P - the one header applications include.
 *
 *     #include "qg4p.h"
 *
 * ARCHITECTURE (top to bottom; each layer only calls the one below it)
 *
 *   Application
 *   Public API ........ qg_screen.h, qg_draw.h, qg_draw_pct.h, qg_block.h, qg_text.h,
 *                       qg_image.h, qg_palette.h
 *   Backends .......... backend/  (DIRECT, BUF8 framebuffer)
 *   Panel drivers ..... drivers/  (ST7789, ILI9341, ST7796S)
 *   HAL ............... hal/      (shared SPI bus, DMA, GPIO)
 *
 * See README.md for an overview, and tools/ for the font, image, asset-pack
 * and size-report scripts.
 */
#ifndef QG4P_H
#define QG4P_H

#include "qg_config.h"
#include "qg_types.h"
#include "hal/qg_hal.h"
#include "qg_screen.h"
#include "qg_palette.h"
#include "qg_draw.h"
#include "qg_draw_pct.h"
#include "qg_block.h"
#include "qg_text.h"
#include "qg_image.h"

#endif /* QG4P_H */
