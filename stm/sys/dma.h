/** @file dma.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "hal.h"

namespace DMA {

    struct HW {
        /** DMA configuration structure */
        struct Conf {
            DMA_Stream_TypeDef *stream;
            DMA_InitTypeDef init;
        };

        /** initialize DMA peripheral */
        HW(const Conf &conf) {
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
}
