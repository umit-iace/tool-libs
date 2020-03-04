/** @file pwm.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_PWM_H
#define STM_PWM_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based PWM derivations
 */
class HardwarePWM {
public:
    /**
     * set pwm in percent
     * @param set [0..100]
     */
    void pwm(double perc, uint32_t chan) {
        __HAL_TIM_SET_COMPARE(&hPWMTim, chan, perc * period / 100.);
    }

    /**
     * Initialize Timer Hardware and PWM pins
     * @param iPinL
     * @param gpioPortL
     * @param iAlternateL
     * @param chanLeft
     * @param iPinR
     * @param gpioPortR
     * @param iAlternateR
     * @param chanRight
     * @param dTim
     */
    HardwarePWM(uint32_t iPinL, GPIO_TypeDef *gpioPortL, uint8_t iAlternateL, uint32_t chanLeft,
             uint32_t iPinR, GPIO_TypeDef *gpioPortR, uint8_t iAlternateR, uint32_t chanRight,
             TIM_TypeDef *dTim) {
        // init pins
        GPIO_InitTypeDef GPIO_InitStruct = {};
        GPIO_InitStruct.Pin = iPinL;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = iAlternateL;
        HAL_GPIO_Init(gpioPortL, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = iPinR;
        GPIO_InitStruct.Alternate = iAlternateR;
        HAL_GPIO_Init(gpioPortR, &GPIO_InitStruct);

        // clock config
        TIM_MasterConfigTypeDef sMasterConfig = {};
        TIM_OC_InitTypeDef sConfigOC = {};

        hPWMTim.Instance = dTim;
        hPWMTim.Init.Prescaler =  0;
        hPWMTim.Init.CounterMode = TIM_COUNTERMODE_UP;
        hPWMTim.Init.Period = period;
        hPWMTim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        hPWMTim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        while (HAL_TIM_PWM_Init(&hPWMTim) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(&hPWMTim, &sMasterConfig) != HAL_OK);

        sConfigOC.OCMode = TIM_OCMODE_PWM1;
        sConfigOC.Pulse = 0;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        while (HAL_TIM_PWM_ConfigChannel(&hPWMTim, &sConfigOC, chanLeft) != HAL_OK);
        while (HAL_TIM_PWM_ConfigChannel(&hPWMTim, &sConfigOC, chanRight) != HAL_OK);

        while (HAL_TIM_PWM_Start(&hPWMTim, chanLeft) != HAL_OK);
        while (HAL_TIM_PWM_Start(&hPWMTim, chanRight) != HAL_OK);
    }

private:
    //\cond false
    TIM_HandleTypeDef hPWMTim = {};
    uint32_t period = 4500 - 1;     // ~ pwm freq of 18600 Hz at 84MHz periph clock
    //\endcond
};

#endif //STM_PWM_H
