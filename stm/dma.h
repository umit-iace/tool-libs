/** @file dma.h
 *
 * Copyright (c) 2022 IACE
 */
#ifndef STM_DMA_H
#define STM_DMA_H
#include "hal.h"

/**
 * initializing wrapper for circular inbound DMA
 */
class HardwareDMA {
public:
    HardwareDMA(DMA_Stream_TypeDef *dma, uint32_t channel) {
        handle.Instance = dma;
        handle.Init = {
                .Channel = channel,
                .Direction = DMA_PERIPH_TO_MEMORY,
                .PeriphInc = DMA_PINC_DISABLE,
                .MemInc = DMA_MINC_ENABLE,
                .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,
                .MemDataAlignment = DMA_MDATAALIGN_BYTE,
                .Mode = DMA_CIRCULAR,
                .Priority = DMA_PRIORITY_LOW,
                .FIFOMode = DMA_FIFOMODE_DISABLE,
        };
        while (HAL_DMA_Init(&handle) != HAL_OK);
    }
    DMA_HandleTypeDef handle{};
};
#endif //STM_DMA_H
