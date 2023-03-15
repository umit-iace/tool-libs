/** @file timer.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once
#include "hal.h"

/** wrapper for timer peripheral */
class HardwareTimer {
public:
    /**
     * Initialize Timer Peripheral
     *
     * @param dTim Timer instance
     * @param prescaler value to write into prescaler register
     * @param period value to write into period register
     */
    HardwareTimer(TIM_TypeDef *dTim, uint32_t prescaler, uint32_t period) {
        TIM_MasterConfigTypeDef sMasterConfig = {};
        handle = {
            .Instance = dTim,
            .Init = {
                .Prescaler = prescaler,
                .CounterMode = TIM_COUNTERMODE_UP,
                .Period = period,
                .ClockDivision = TIM_CLOCKDIVISION_DIV1,
                .AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
            },
        };
        while (HAL_TIM_Base_Init(&handle) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(&handle, &sMasterConfig) != HAL_OK);
    }

    /**
     * configure callback on PERIOD ELAPSED event
     * @param callback function to be called
     */
    void configCallback(void (*callback)(TIM_HandleTypeDef *)) {
        HAL_TIM_RegisterCallback(&handle, HAL_TIM_PERIOD_ELAPSED_CB_ID, callback);
    }

    /** start timer in interrupt mode */
    void start() {
        while (HAL_TIM_Base_Start_IT(&handle) != HAL_OK);
    }

    /** call directly in interrupt */
    void irqHandler() {
        HAL_TIM_IRQHandler(&handle);
    }

    /** HAL handle, for use with unwrapped Timer capabilities */
    TIM_HandleTypeDef handle = {};
};
