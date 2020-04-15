/** @file servo.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_SERVO_H
#define STM_SERVO_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based PWM derivations
 */
class Servo {
public:
    /**
     * set servo in percent
     * @param perc set [-100..100]
     */
    void left(double perc) {
        uint16_t timing = percentToPWM(perc);
        __HAL_TIM_SET_COMPARE(&hPWMTim, iCleft, timing);
    }

    /**
    * set servo in percent
    * @param perc set [-100..100]
    */
    void right(double perc) {
        uint16_t timing = percentToPWM(perc);
        __HAL_TIM_SET_COMPARE(&hPWMTim, iCright, timing);
    }

    /**
     * calculate pwm timing from percent
     * @param percent servo setting
     * @return PWM Channel setting
     */
    uint16_t percentToPWM(double percent) {
        const unsigned long tMin = 1280;    // us
        const unsigned long tMax = 1720;    // us
        long timing = (tMax + tMin) / 2. + (tMax - tMin) / 200. * percent;  // us
        return timing;
    }

    /**
     * Initialize pins and timer for a pwm output
     *
     * @param iPin1
     * @param gpioPort1
     * @param iAlternate1
     * @param left
     * @param iPin2
     * @param gpioPort2
     * @param iAlternate2
     * @param right
     * @param dTim
     */
    Servo(uint32_t iPin1, GPIO_TypeDef *gpioPort1, uint8_t iAlternate1, uint32_t left,
             uint32_t iPin2, GPIO_TypeDef *gpioPort2, uint8_t iAlternate2, uint32_t right,
             TIM_TypeDef *dTim) :
                 iCleft(left), iCright(right) {
        // init pins
        GPIO_InitTypeDef GPIO_InitStruct = {};
        GPIO_InitStruct.Pin = iPin1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = iAlternate1;
        HAL_GPIO_Init(gpioPort1, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = iPin2;
        GPIO_InitStruct.Alternate = iAlternate2;
        HAL_GPIO_Init(gpioPort2, &GPIO_InitStruct);

        // clock config
        TIM_MasterConfigTypeDef sMasterConfig = {};
        TIM_OC_InitTypeDef sConfigOC = {};

        hPWMTim.Instance = dTim;
        hPWMTim.Init.Prescaler = 108 + 1; //??? TODO: why is this better than theoretically correct 108-1???
        hPWMTim.Init.CounterMode = TIM_COUNTERMODE_UP;
        hPWMTim.Init.Period = 20000 - 1;
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
        while (HAL_TIM_PWM_ConfigChannel(&hPWMTim, &sConfigOC, iCleft) != HAL_OK);
        while (HAL_TIM_PWM_ConfigChannel(&hPWMTim, &sConfigOC, iCright) != HAL_OK);
        while (HAL_TIM_PWM_Start(&hPWMTim, iCleft) != HAL_OK);
        while (HAL_TIM_PWM_Start(&hPWMTim, iCright) != HAL_OK);
        this->right(0);
        this->left(0);
        while (HAL_TIM_Base_Start(&hPWMTim));
    }

private:
    //\cond false
    TIM_HandleTypeDef hPWMTim = {};
    uint32_t iCleft = 0;
    uint32_t iCright = 0;
    //\endcond
};

#endif //STM_SERVO_H

