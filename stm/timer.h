/** @file timer.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_TIMER_H
#define STM_TIMER_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based Timers
 */
class HardwareTimer {
public:
    /**
     * Initialize Timer Hardware
     *
     * @param dTim Timer instance
     * @param prescaler value to write into prescaler register
     * @param period value to write into period register
     */
    HardwareTimer(TIM_TypeDef *dTim, uint32_t prescaler, uint32_t period) {
        this->hTim.Instance = dTim;
        TIM_MasterConfigTypeDef sMasterConfig = {};
        hTim.Init.Prescaler = prescaler;
        hTim.Init.CounterMode = TIM_COUNTERMODE_UP;
        hTim.Init.Period = period;
        hTim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        hTim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        while (HAL_TIM_Base_Init(&hTim) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(&hTim, &sMasterConfig) != HAL_OK);
    }

    /**
     * return HAL handle to timer object
     */
    TIM_HandleTypeDef *handle() {
        return &hTim;
    }

    /**
     * configure callback on PERIOD ELAPSED event
     * @param callback function to be called
     * @param irq interrupt number
     * @param pre priority
     * @param sub priority
     */
    void configCallback(void (*callback)(TIM_HandleTypeDef *)) {
        HAL_TIM_RegisterCallback(&hTim, HAL_TIM_PERIOD_ELAPSED_CB_ID, callback);
    }

    /**
     * start timer in interrupt mode
     */
    void start() {
        while (HAL_TIM_Base_Start_IT(&hTim) != HAL_OK);
    }

private:
    //\cond false
    TIM_HandleTypeDef hTim = {};
    //\endcond
};

#endif //STM_TIMER_H
