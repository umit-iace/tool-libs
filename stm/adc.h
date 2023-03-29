/** @file adc.h
 *
 * Copyright (c) 2021 IACE
 */

#ifndef STM_ADC_H
#define STM_ADC_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based ADC with DMA
 */
class HardwareADC {
public:
    HardwareADC(uint8_t iBufferCount,
                ADC_HandleTypeDef *hAdc, DMA_HandleTypeDef *hDMA)
                : hTDAdc(hAdc), hTDAdcDMA(hDMA), iBufferCount(iBufferCount) {
        pThis = this;
        iBuffer = new uint32_t[iBufferCount];
        for(uint8_t i = 0; i < iBufferCount; ++i) {
            iBuffer[i] = 0;
        }
    }

    //\cond false
    ADC_HandleTypeDef *hTDAdc;
    DMA_HandleTypeDef *hTDAdcDMA;
    //\endcond

protected:
    virtual void adcCallback(ADC_HandleTypeDef *hAdc) {}

    void configChannel(uint32_t iChannel, uint8_t iRank, uint32_t iSamplingTime) const {
        ADC_ChannelConfTypeDef sConfig = {};
        sConfig.Channel = iChannel;
        sConfig.Rank = iRank;
        sConfig.SamplingTime = iSamplingTime;
        while (HAL_ADC_ConfigChannel(this->hTDAdc, &sConfig) != HAL_OK);
    }

    void configADC(ADC_TypeDef *dADC) const {
        *this->hTDAdc = {};
        this->hTDAdc->Instance = dADC;
        this->hTDAdc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
        this->hTDAdc->Init.Resolution = ADC_RESOLUTION_12B;
        this->hTDAdc->Init.ScanConvMode = ENABLE;
        this->hTDAdc->Init.ContinuousConvMode = DISABLE;
        this->hTDAdc->Init.DiscontinuousConvMode = DISABLE;
        this->hTDAdc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
        this->hTDAdc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
        this->hTDAdc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
        this->hTDAdc->Init.NbrOfConversion = this->iBufferCount;
        this->hTDAdc->Init.DMAContinuousRequests = ENABLE;
        this->hTDAdc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
        while (HAL_ADC_Init(this->hTDAdc) != HAL_OK);
    }

    void configDMA(DMA_Stream_TypeDef *dDMA, uint32_t iDMAChannel,
                   IRQn_Type iAdcDmaInterrupt, uint32_t iAdcDmaPrePrio, uint32_t iAdcDmaSubPrio) {
        *this->hTDAdcDMA = {};
        this->hTDAdcDMA->Instance = dDMA;
        this->hTDAdcDMA->Init.Channel = iDMAChannel;
        this->hTDAdcDMA->Init.Direction = DMA_PERIPH_TO_MEMORY;
        this->hTDAdcDMA->Init.PeriphInc = DMA_PINC_DISABLE;
        this->hTDAdcDMA->Init.MemInc = DMA_MINC_ENABLE;
        this->hTDAdcDMA->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        this->hTDAdcDMA->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        this->hTDAdcDMA->Init.Mode = DMA_CIRCULAR;
        this->hTDAdcDMA->Init.Priority = DMA_PRIORITY_MEDIUM;
        this->hTDAdcDMA->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        while(HAL_DMA_Init(this->hTDAdcDMA) != HAL_OK);

        __HAL_LINKDMA(this->hTDAdc, DMA_Handle, *this->hTDAdcDMA);

        HAL_ADC_RegisterCallback(this->hTDAdc, HAL_ADC_CONVERSION_COMPLETE_CB_ID, conversionComplete);

        HAL_NVIC_SetPriority(iAdcDmaInterrupt, iAdcDmaPrePrio, iAdcDmaSubPrio);
        HAL_NVIC_EnableIRQ(iAdcDmaInterrupt);

        /// start
        while(HAL_ADC_Start_DMA(this->hTDAdc, this->iBuffer, this->iBufferCount) != HAL_OK);
    }

    void startMeasurement() const {
        HAL_ADC_Start(this->hTDAdc);
    }

    uint32_t* iBuffer;

private:
    uint8_t iBufferCount = 0;
    inline static HardwareADC *pThis = nullptr;

    static void conversionComplete(ADC_HandleTypeDef *hAdc) {
        pThis->adcCallback(hAdc);
    }
};

#endif //STM_ADC_H/** @file adc.h
 *
 * Copyright (c) 2021 IACE
 */

#ifndef STM_ADC_H
#define STM_ADC_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based ADC with DMA
 */
class HardwareADC {
public:
    HardwareADC(uint8_t iBufferCount,
                ADC_HandleTypeDef *hAdc, DMA_HandleTypeDef *hDMA)
                : hTDAdc(hAdc), hTDAdcDMA(hDMA), iBufferCount(iBufferCount) {
        pThis = this;
        iBuffer = new uint32_t[iBufferCount];
        for(uint8_t i = 0; i < iBufferCount; ++i) {
            iBuffer[i] = 0;
        }
    }

    //\cond false
    ADC_HandleTypeDef *hTDAdc;
    DMA_HandleTypeDef *hTDAdcDMA;
    //\endcond

protected:
    virtual void adcCallback(ADC_HandleTypeDef *hAdc) {}

    void configChannel(uint32_t iChannel, uint8_t iRank, uint32_t iSamplingTime) const {
        ADC_ChannelConfTypeDef sConfig = {};
        sConfig.Channel = iChannel;
        sConfig.Rank = iRank;
        sConfig.SamplingTime = iSamplingTime;
        while (HAL_ADC_ConfigChannel(this->hTDAdc, &sConfig) != HAL_OK);
    }

    void configADC(ADC_TypeDef *dADC) const {
        *this->hTDAdc = {};
        this->hTDAdc->Instance = dADC;
        this->hTDAdc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
        this->hTDAdc->Init.Resolution = ADC_RESOLUTION_12B;
        this->hTDAdc->Init.ScanConvMode = ENABLE;
        this->hTDAdc->Init.ContinuousConvMode = DISABLE;
        this->hTDAdc->Init.DiscontinuousConvMode = DISABLE;
        this->hTDAdc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
        this->hTDAdc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
        this->hTDAdc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
        this->hTDAdc->Init.NbrOfConversion = this->iBufferCount;
        this->hTDAdc->Init.DMAContinuousRequests = ENABLE;
        this->hTDAdc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
        while (HAL_ADC_Init(this->hTDAdc) != HAL_OK);
    }

    void configDMA(DMA_Stream_TypeDef *dDMA, uint32_t iDMAChannel,
                   IRQn_Type iAdcDmaInterrupt, uint32_t iAdcDmaPrePrio, uint32_t iAdcDmaSubPrio) {
        *this->hTDAdcDMA = {};
        this->hTDAdcDMA->Instance = dDMA;
        this->hTDAdcDMA->Init.Channel = iDMAChannel;
        this->hTDAdcDMA->Init.Direction = DMA_PERIPH_TO_MEMORY;
        this->hTDAdcDMA->Init.PeriphInc = DMA_PINC_DISABLE;
        this->hTDAdcDMA->Init.MemInc = DMA_MINC_ENABLE;
        this->hTDAdcDMA->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        this->hTDAdcDMA->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        this->hTDAdcDMA->Init.Mode = DMA_CIRCULAR;
        this->hTDAdcDMA->Init.Priority = DMA_PRIORITY_MEDIUM;
        this->hTDAdcDMA->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        while(HAL_DMA_Init(this->hTDAdcDMA) != HAL_OK);

        __HAL_LINKDMA(this->hTDAdc, DMA_Handle, *this->hTDAdcDMA);

        HAL_ADC_RegisterCallback(this->hTDAdc, HAL_ADC_CONVERSION_COMPLETE_CB_ID, conversionComplete);

        HAL_NVIC_SetPriority(iAdcDmaInterrupt, iAdcDmaPrePrio, iAdcDmaSubPrio);
        HAL_NVIC_EnableIRQ(iAdcDmaInterrupt);

        /// start
        while(HAL_ADC_Start_DMA(this->hTDAdc, this->iBuffer, this->iBufferCount) != HAL_OK);
    }

    void startMeasurement() const {
        HAL_ADC_Start(this->hTDAdc);
    }

    uint32_t* iBuffer;

private:
    uint8_t iBufferCount = 0;
    inline static HardwareADC *pThis = nullptr;

    static void conversionComplete(ADC_HandleTypeDef *hAdc) {
        pThis->adcCallback(hAdc);
    }
};

#endif //STM_ADC_H
