/** @file pwm.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include "hal.h"
#include "gpio.h"
#include "timer.h"

namespace TIMER {
    /**
     * @brief Template class for hardware based PWM outputs
     */
    class PWM {
    public:
        /**
         * set pwm in percent
         * @param perc [0..1]
         */
        void pwm(double perc) {
            _perc = perc;
            _ticks = perc * _period;
            __HAL_TIM_SET_COMPARE(&hTim, chan, _ticks);
        }

        /**
         * set pwm as value of timer ticks
         * @param ticks [0..TIMER::HW period]
         */
        void ticks(uint32_t ticks) {
            _perc = ticks / _period;
            _ticks = ticks;
            __HAL_TIM_SET_COMPARE(&hTim, chan, _ticks);
        }

        /**
         * change frequency by setting period of timer
         * freq = base / period
         * counter is blocked while period = 0
         * @param period []
         */
        void period(uint16_t period) {
            if (period == _period) return;
            bool needupdate = _period == 0;
            _period = period;
            hTim.Instance->CR1 |= TIM_CR1_UDIS;
            hTim.Instance->ARR = _period;
            pwm(_perc);
            hTim.Instance->CR1 &= ~TIM_CR1_UDIS;
            if (needupdate || hTim.Instance->CNT >= _period)
                hTim.Instance->EGR |= TIM_EGR_UG;
        }

        uint16_t period() {
            return _period;
        }

        /** PWM configuration */
        struct Config {
            TIMER::HW *tim; ///< TIMER::HW peripheral
            uint32_t chan; ///< Timer channel
            AFIO pin; ///< pin in alternate function mode
        };

        /**
         * Initialize Timer Channel
         */
        PWM(const Config &conf)
                : hTim(conf.tim->handle), chan(conf.chan), _period(hTim.Init.Period) {
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

        /** get currently set pwm value in percent */
        double pwm(){
            return _perc;
        }
        double getpwm(){
            return pwm();
        }
        /** get currently set pwm value in timer ticks */
        uint32_t ticks(){
            return _ticks;
        }
        uint32_t getticks(){
            return ticks();
        }

    private:
        TIM_HandleTypeDef &hTim;
        uint32_t chan = 0;
        uint32_t &_period;
        double _perc = 0.0;
        uint32_t _ticks = 0;
    };
}
