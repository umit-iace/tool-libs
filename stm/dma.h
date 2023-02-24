/** @file dma.h
 *
 * Copyright (c) 2022 IACE
 */
#pragma once
#include "hal.h"

/** initializing wrapper for DMA peripheral */
class HardwareDMA {
public:
    /** DMA configuration structure */
    struct Conf {
        DMA_Stream_TypeDef *stream;
        DMA_InitTypeDef init;
    };
    /** initialize DMA peripheral */
    HardwareDMA(Conf conf) {
        handle.Instance = conf.stream;
        handle.Init = conf.init;
        while (HAL_DMA_Init(&handle) != HAL_OK);
    }
    /** call directly in interrupt */
    void irqHandler() {
        HAL_DMA_IRQHandler(&handle);
    }
    /** HAL handle, for use with unwrapped DMA capabilities */
    DMA_HandleTypeDef handle{};
};
