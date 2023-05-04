/** @file adc.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

#include "gpio.h"
#include "registry.h"

namespace Adc {

    struct HW {
        inline static Registry<HW, ADC_HandleTypeDef, 4> reg{};

        struct ChannelConf {
            uint32_t channel;
            uint8_t rank;
            uint32_t sampleTime;
            DIO pin;
        };

        struct Conf {
            ADC_TypeDef *adc;
            DMA_Stream_TypeDef *dmaStream;
            uint32_t dmaChannel;
            IRQn_Type adcDmaInterrupt;
            uint32_t adcDmaPrePrio;
            uint32_t adcDmaSubPrio;
            uint8_t adcChannelCount;
            ChannelConf channels[2];
        };

        HW(const Conf &conf);

        void configADC(ADC_TypeDef *dADC);
        void configDMA(DMA_Stream_TypeDef *dDMA, uint32_t iDMAChannel,
                       IRQn_Type iAdcDmaInterrupt, uint32_t iAdcDmaPrePrio,
                       uint32_t iAdcDmaSubPrio);
        void configChannel(const ChannelConf &conf );
        void irqHandler(void);

        void measure() {
            HAL_ADC_Start(&hAdc);
        }

        double getBuffer(uint8_t idx) {
            return this->iBuffer[idx];
        }

    private:
        uint8_t adcChannelCount = 0;
        ADC_HandleTypeDef hAdc{};
        DMA_HandleTypeDef hDma{};
        uint32_t *iBuffer;
    };
}
