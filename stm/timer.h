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
     * @param dTim
     * @param hTim
     * @param callback
     */
    HardwareTimer(TIM_HandleTypeDef *hTim, TIM_TypeDef *dTim) : hTim(hTim) {
        this->hTim->Instance = dTim;
    }

    void configCallback(void (*callback)(TIM_HandleTypeDef *),IRQn_Type irq, uint32_t pre, uint32_t sub) {
        HAL_TIM_RegisterCallback(this->hTim, HAL_TIM_PERIOD_ELAPSED_CB_ID, callback);
        HAL_NVIC_SetPriority(irq, pre, sub);
        HAL_NVIC_EnableIRQ(irq);
    }

    void startTimer(uint32_t presc, uint32_t timeout) {
        TIM_MasterConfigTypeDef sMasterConfig = {};
        hTim->Init.Prescaler = presc - 1;
        hTim->Init.CounterMode = TIM_COUNTERMODE_UP;
        hTim->Init.Period = timeout - 1;
        hTim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        hTim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        while (HAL_TIM_Base_Init(hTim) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(hTim, &sMasterConfig) != HAL_OK);

        while (HAL_TIM_Base_Start_IT(hTim) != HAL_OK);
    }
private:
    //\cond false
    TIM_HandleTypeDef *hTim = nullptr;
    //\endcond
};

#endif //STM_TIMER_H
