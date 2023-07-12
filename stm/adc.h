/** @file adc.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

#include "gpio.h"
#include "dma.h"

namespace ADC {
    /** Class representing one measured pin/value. */
    struct Channel {
        /** return measured value */
        double get() {
            return raw * lsb;
        }
        /** raw value from adc */
        int16_t &raw;
        const double lsb;
    };

    /** Analog to Digital Converter Peripheral Driver */
    struct HW {
        /** driver configuration */
        struct Conf {
            ADC_TypeDef *adc; ///< ADC Peripheral
            uint32_t prescaler; ///< Peripheral clock prescaler
            DMA_Stream_TypeDef *dmaStream; ///< DMA Stream for reading out raw values
            uint32_t dmaChannel; ///< DMA Channel
        };
        /** channel configuration */
        struct ChannelConf {
            uint32_t channel; ///< ADC Channel number [i.e. Pin]
            uint32_t samplingTime; ///< sampling time
            double lsb; ///< Least Significant Bit value factor
            DIO pin; ///< Analog configured pin
        };

        /** create Driver with given Conf */
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

        /** start measuring all registered channels/pins */
        void measure() {
            while (HAL_ADC_Start_DMA(&handle, (uint32_t*)iBuffer, channelCnt) != HAL_OK);
        }

        /** initialize driver.
         * call _after_ registering/creating all the needed Channel&zwj;s
         */
        void init() {
            while (HAL_ADC_Init(&handle) != HAL_OK);
            __HAL_LINKDMA(&handle, DMA_Handle, dma.handle);
        }

        /** register/create pin/channel for analog measurement */
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
