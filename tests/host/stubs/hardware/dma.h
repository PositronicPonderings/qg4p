/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
typedef struct { uint32_t ctrl; } dma_channel_config;
enum dma_channel_transfer_size { DMA_SIZE_8, DMA_SIZE_16, DMA_SIZE_32 };
dma_channel_config dma_channel_get_default_config(uint);
void channel_config_set_transfer_data_size(dma_channel_config*, enum dma_channel_transfer_size);
void channel_config_set_dreq(dma_channel_config*, uint);
void channel_config_set_read_increment(dma_channel_config*, bool);
void channel_config_set_write_increment(dma_channel_config*, bool);
void dma_channel_configure(uint, const dma_channel_config*, volatile void*, const volatile void*, uint32_t, bool);
void dma_channel_wait_for_finish_blocking(uint);
int dma_claim_unused_channel(bool);
