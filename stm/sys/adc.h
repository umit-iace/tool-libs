/** @file adc.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

#include "gpio.h"
#include "dma.h"

#if defined(ADC)
// workaround for Legacy HAL defining 'ADC' as the common base between the
// different ADCs. current HAL defines and uses it as 'ADC123_COMMON', so
// we can safely discard it
#undef ADC
#endif

namespace ADC {
    /** number of channels a HW can convert */
    constexpr uint32_t MAXCHANNELS = 16;
    /** DMA target, a whole number of cache lines (Cortex-M7: 32 bytes) */
    struct alignas(32) Samples {
        int16_t raw[MAXCHANNELS];
        /** the DMA writes behind the CPU's back, drop stale cache lines */
        void invalidate() {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
            SCB_InvalidateDCache_by_Addr((uint32_t *)raw, sizeof raw);
#endif
        }
        /** before handing the buffer to the DMA: write back what the CPU
         * wrote, so no dirty line overwrites the samples later on */
        void flush() {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)raw, sizeof raw);
#endif
        }
    };
    static_assert(sizeof(Samples) % 32 == 0);

    /** Class representing one measured pin/value. */
    struct Channel {
        /** return measured value */
        double get() {
            if (!samples) return 0;
            samples->invalidate();
            return samples->raw[ix] * lsb;
        }
        Samples *samples; ///< nullptr if the ADC had no channel left
        uint32_t ix;
        double lsb;
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
            samples.flush();
            while (HAL_ADC_Start_DMA(&handle, (uint32_t*)samples.raw, channelCnt) != HAL_OK);
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
            assert(channelCnt < MAXCHANNELS);
            if (channelCnt >= MAXCHANNELS) return {nullptr, 0, 0};
            auto ret = Channel{&samples, channelCnt, conf.lsb};

            ADC_ChannelConfTypeDef sConfig {
                .Channel = conf.channel,
                .Rank = channelCnt + 1,
                .SamplingTime = conf.samplingTime,
            };
            while (HAL_ADC_ConfigChannel(&handle, &sConfig) != HAL_OK);

            channelCnt++;
            // HAL_ADC_Init wrote the sequence length when there were no
            // channels yet, which the hardware takes as 16 conversions
            MODIFY_REG(handle.Instance->SQR1, ADC_SQR1_L,
                    (channelCnt - 1) << ADC_SQR1_L_Pos);

            return ret;
        }

        ADC_HandleTypeDef handle{};
        Samples samples{};
        uint32_t &channelCnt = handle.Init.NbrOfConversion;
        DMA::HW dma;
    };
}
