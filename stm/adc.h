/** @file adc.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

#include "gpio.h"
#include "dma.h"

namespace Adc {

    struct Channel {
        double get() {
            return raw * lsb;
        }
        int16_t &raw;
        const double lsb;
    };

    struct HW {
        struct Conf {
            ADC_TypeDef *adc;
            uint32_t prescaler;
            DMA_Stream_TypeDef *dmaStream;
            uint32_t dmaChannel;
        };
        struct ChannelConf {
            uint32_t channel;
            uint32_t samplingTime;
            double lsb;
            DIO pin;
        };

        HW(const Conf &conf) 
                : dma {{
                    .stream = conf.dmaStream,
                    .init = {
                        .Channel = conf.dmaChannel,
                        .MemInc = DMA_MINC_ENABLE,
                        .PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD,
                        .MemDataAlignment = DMA_MDATAALIGN_HALFWORD,
                        .Mode = DMA_CIRCULAR,
                    }
                }}
        {
            handle.Instance = conf.adc;
            handle.Init = {
                .ClockPrescaler = conf.prescaler,
                .ScanConvMode = ENABLE,
                .ExternalTrigConv = ADC_SOFTWARE_START,
                .DMAContinuousRequests = ENABLE,
            };
        }

        void measure() {
            while (HAL_ADC_Start_DMA(&handle, (uint32_t*)iBuffer, channelCnt) != HAL_OK);
        }

        void init() {
            while (HAL_ADC_Init(&handle) != HAL_OK);
            __HAL_LINKDMA(&handle, DMA_Handle, dma.handle);
        }

        Channel get(ChannelConf conf){
            auto ret = Channel{iBuffer[channelCnt], conf.lsb};

            ADC_ChannelConfTypeDef sConfig {
                .Channel = conf.channel,
                .Rank = channelCnt + 1,
                .SamplingTime = conf.samplingTime,
            };
            while (HAL_ADC_ConfigChannel(&handle, &sConfig) != HAL_OK);

            channelCnt++;

            return ret;
        }

        ADC_HandleTypeDef handle{};
        int16_t iBuffer[16];
        uint32_t &channelCnt = handle.Init.NbrOfConversion;
        DMA::HW dma;
    };
}
