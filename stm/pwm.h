/** @file pwm.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include "hal.h"
#include "gpio.h"
#include "timer.h"

/**
 * @brief Template class for hardware based PWM outputs
 */
namespace PWM {

class HW {
public:
    /**
     * set pwm in percent
     * @param perc [0..1]
     */
    void pwm(double perc) {
        ticks(perc * period);
    }

    /**
     * set pwm as value of timer ticks
     * @param ticks
     */
     void ticks(uint32_t ticks) {
         __HAL_TIM_SET_COMPARE(&hTim, chan, ticks);
     }

     struct Config {
         HardwareTimer tim;
         uint32_t chan;
         AFIO pin;
     };
    /**
     * Initialize Timer Channel
     *
     * make sure to initialize the corresponding AFIO pin
     *
     * @param tim pointer to HardwareTimer
     * @param chan timer channel number (TIM_CHANNEL_x)
     */
    HW(const Config &conf)
        : hTim(conf.tim.handle)
        , chan(conf.chan)
        , period(hTim.Init.Period)
    {
        // pwm config of timer
        while (HAL_TIM_PWM_Init(&hTim) != HAL_OK);

        // channel config
        TIM_OC_InitTypeDef sConfigOC = {};
        sConfigOC.OCMode = TIM_OCMODE_PWM1;
        sConfigOC.Pulse = 0;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        while (HAL_TIM_PWM_ConfigChannel(&hTim, &sConfigOC, chan) != HAL_OK);

        __HAL_TIM_SET_COMPARE(&hTim, chan, 0);
        while (HAL_TIM_PWM_Start(&hTim, chan) != HAL_OK);
    }

private:
    TIM_HandleTypeDef hTim{};
    uint32_t chan= 0;
    uint32_t period = 0;
};
}
