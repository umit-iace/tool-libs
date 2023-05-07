/** @file adc.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

#include "gpio.h"
#include "registry.h"
#include "dma.h"

namespace Adc {

    struct Channel {
        struct Conf {
            ADC_ChannelConfTypeDef conf;
            DIO pin;
        };
        Channel(const Conf &conf) : conf(conf.conf), pin(conf.pin) {}

        uint32_t get() {
            return *value;
        }

        ADC_ChannelConfTypeDef conf;
        uint32_t *value = nullptr;
        DIO pin;
    };

    struct HW {
        struct Conf {
            ADC_TypeDef *adc;
            ADC_InitTypeDef init;
            DMA_HandleTypeDef dmaHandle;
        };

        HW(const Conf &conf) : dmaHandle(conf.dmaHandle) {
            handle.Instance = conf.adc;
            handle.Init = conf.init;
            while (HAL_ADC_Init(&this->handle) != HAL_OK);
        }

        void irqHandler() {
            HAL_ADC_IRQHandler(&handle);
        }

        void measure() {
            HAL_ADC_Start(&handle);
        }

        void init() {
            __HAL_LINKDMA(&this->handle, DMA_Handle, this->dmaHandle);
            while (HAL_ADC_Start_DMA(&this->handle, this->iBuffer, this->channelCnt) != HAL_OK);
        }

        Channel getChannel(Channel::Conf conf){
            Channel channel = Channel{conf};
            channel.value = &this->iBuffer[this->channelCnt];

            ADC_ChannelConfTypeDef sConfig = conf.conf;
            while (HAL_ADC_ConfigChannel(&this->handle, &sConfig) != HAL_OK);

            this->channelCnt++;

            return channel;
        }

        void configCallback(void (*callback)(ADC_HandleTypeDef *)) {
            HAL_ADC_RegisterCallback(&handle, HAL_ADC_CONVERSION_COMPLETE_CB_ID, callback);
        }

    private:
        ADC_HandleTypeDef handle{};
        uint32_t iBuffer[16];
        uint8_t channelCnt = 0;
        DMA_HandleTypeDef dmaHandle;
    };
}
