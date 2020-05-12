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
     * @param perc [0..1]
     * @param chan Timer channel (TIM_CHANNEL_x)
     */
    void pwm(double perc, uint32_t chan) {
        __HAL_TIM_SET_COMPARE(&hPWMTim, chan, perc * period);
    }

    /**
     * Initialize Timer Hardware and PWM pins
     *
     * @param dTim TIMx
     * @param prescaler 16bit value
     * @param period 16bit value
     *
     * Timer counts with frequency of `TIM_periph / prescaler` from 0 to `period`
     */
    HardwarePWM(TIM_TypeDef *dTim, uint32_t prescaler, uint32_t period) : period(period) {
        // clock config
        TIM_MasterConfigTypeDef sMasterConfig = {};

        hPWMTim.Instance = dTim;
        hPWMTim.Init.Prescaler = prescaler;
        hPWMTim.Init.CounterMode = TIM_COUNTERMODE_UP;
        hPWMTim.Init.Period = period;
        hPWMTim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        hPWMTim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        while (HAL_TIM_PWM_Init(&hPWMTim) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(&hPWMTim, &sMasterConfig) != HAL_OK);
    }

    /**
     * enable PWM output on given timer channel
     * @param iPin pin number of PWM output
     * @param gpioPort pin port of PWM output
     * @param iAlternate alternate function for PWM output
     * @param chan timer channel number (TIM_CHANNEL_x)
     */
    void startChannel(uint32_t iPin, GPIO_TypeDef *gpioPort, uint8_t iAlternate, uint32_t chan)
    {
        // init pins
        GPIO_InitTypeDef GPIO_InitStruct = {};
        GPIO_InitStruct.Pin = iPin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = iAlternate;
        HAL_GPIO_Init(gpioPort, &GPIO_InitStruct);

        TIM_OC_InitTypeDef sConfigOC = {};
        sConfigOC.OCMode = TIM_OCMODE_PWM1;
        sConfigOC.Pulse = 0;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        while (HAL_TIM_PWM_ConfigChannel(&hPWMTim, &sConfigOC, chan) != HAL_OK);

        while (HAL_TIM_PWM_Start(&hPWMTim, chan) != HAL_OK);
    }

private:
    //\cond false
    TIM_HandleTypeDef hPWMTim = {};
    uint32_t period = 0;
    //\endcond
};

#endif //STM_PWM_H
