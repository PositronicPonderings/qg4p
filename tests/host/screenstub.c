/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include "qg_screen.h"
void qg_screen_set_line_width(qg_screen_t *scr, uint8_t w){ scr->line_width = w<1?1:w; }
