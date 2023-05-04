/** @file adc.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "adc.h"

namespace Adc {
}

using namespace Adc;

void conversionComplete(ADC_HandleTypeDef *handle) {
    auto adc = HW::reg.from(handle);
}

HW::HW(const Conf &conf) {
    iBuffer = new uint32_t[conf.adcChannelCount];
    for (uint8_t i = 0; i < conf.adcChannelCount; ++i) {
        iBuffer[i] = 0;
        this->configChannel(conf.channels[i]);
    }

    this->configADC(conf.adc);
    this->configDMA(conf.dmaStream, conf.dmaChannel,
                    conf.adcDmaInterrupt,conf.adcDmaInterrupt,
                    (conf.adcDmaPrePrio, conf.adcDmaSubPrio));
};

void HW::configChannel(const ChannelConf &conf ) {
    ADC_ChannelConfTypeDef sConfig = {};
    sConfig.Channel = conf.channel;
    sConfig.Rank = conf.rank;
    sConfig.SamplingTime = conf.sampleTime;
    while (HAL_ADC_ConfigChannel(&this->hAdc, &sConfig) != HAL_OK);
}

void HW::configADC(ADC_TypeDef *dADC) {
    this->hAdc.Instance = dADC;
    this->hAdc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    this->hAdc.Init.Resolution = ADC_RESOLUTION_12B;
    this->hAdc.Init.ScanConvMode = ENABLE;
    this->hAdc.Init.ContinuousConvMode = DISABLE;
    this->hAdc.Init.DiscontinuousConvMode = DISABLE;
    this->hAdc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    this->hAdc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    this->hAdc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    this->hAdc.Init.NbrOfConversion = this->adcChannelCount;
    this->hAdc.Init.DMAContinuousRequests = ENABLE;
    this->hAdc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    while (HAL_ADC_Init(&this->hAdc) != HAL_OK);
}

void HW::configDMA(DMA_Stream_TypeDef *dDMA, uint32_t iDMAChannel,
               IRQn_Type iAdcDmaInterrupt, uint32_t iAdcDmaPrePrio, uint32_t iAdcDmaSubPrio) {
    this->hDma.Instance = dDMA;
    this->hDma.Init.Channel = iDMAChannel;
    this->hDma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    this->hDma.Init.PeriphInc = DMA_PINC_DISABLE;
    this->hDma.Init.MemInc = DMA_MINC_ENABLE;
    this->hDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    this->hDma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    this->hDma.Init.Mode = DMA_CIRCULAR;
    this->hDma.Init.Priority = DMA_PRIORITY_MEDIUM;
    this->hDma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    while (HAL_DMA_Init(&this->hDma) != HAL_OK);

    __HAL_LINKDMA(&this->hAdc, DMA_Handle, this->hDma);

    HAL_ADC_RegisterCallback(&this->hAdc,
                             HAL_ADC_CONVERSION_COMPLETE_CB_ID,
                             conversionComplete);

    HAL_NVIC_SetPriority(iAdcDmaInterrupt, iAdcDmaPrePrio, iAdcDmaSubPrio);
    HAL_NVIC_EnableIRQ(iAdcDmaInterrupt);

    /// start
    while (HAL_ADC_Start_DMA(&this->hAdc, this->iBuffer, this->adcChannelCount) != HAL_OK);
}

void HW::irqHandler(void) {
    HAL_DMA_IRQHandler(&this->hDma);
}
