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
    struct Conf {
        DMA_Stream_TypeDef *stream;
        DMA_InitTypeDef init;
    };
    HardwareDMA(Conf conf) {
        handle.Instance = conf.stream;
        handle.Init = conf.init;
        while (HAL_DMA_Init(&handle) != HAL_OK);
    }
    void irqHandler() {
        HAL_DMA_IRQHandler(&handle);
    }
    DMA_HandleTypeDef handle{};
};
#endif //STM_DMA_H
