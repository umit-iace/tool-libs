/** @file encoder.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once

#include "gpio.h"
#include "hal.h"
#include "timer.h"

namespace TIMER {
    /** Quadrature Encoder support */
    class Encoder {
    public:
        /**
         * Encoder constructor.
         *
         * make sure the pins for the encoder are on
         * channels 1 and 2 of the used timer.
         *
         * @param tim Microcontroller Timer Peripheral (e.g. TIM2)
         * @param a Alternate Function initialized pin A
         * @param b Alternate Function initialized pin B
         * @param factor all encompassing conversion factor from encoder value
         * to needed unit\n
         * Examples:
         *  * 4-bit/revolution rotary encoder, position in \c degrees:
         *    \verbatim factor = 1 / pow(2, 4) * 360; \endverbatim
         *  * 12-bit/revolution rotary encoder, position in \c radiants:
         *    \verbatim factor = 1 / pow(2, 12) * M_PI; \endverbatim
         *  * 10-bit/revolution rotary encoder, position in \c m traveled with
         *    a 6cm radius wheel:
         *    \verbatim factor = 1 / pow(2, 10) * M_PI * 0.06; \endverbatim
         */
        struct Conf {
            TIM_TypeDef *tim;
            AFIO a, b;
            double factor = 1;
            uint8_t filter = 0;
        };

        Encoder(Conf conf) :
                tim(TIMER::HW(conf.tim, 0, 0xffff)),
                dFactor(conf.factor) {

            TIM_HandleTypeDef *htim = &tim.handle;
            TIM_Encoder_InitTypeDef sConfig = {
                .EncoderMode = TIM_ENCODERMODE_TI12,
                .IC1Polarity = TIM_ICPOLARITY_RISING,
                .IC1Selection = TIM_ICSELECTION_DIRECTTI,
                .IC1Prescaler = TIM_ICPSC_DIV1,
                .IC1Filter = conf.filter,
                .IC2Polarity = TIM_ICPOLARITY_RISING,
                .IC2Selection = TIM_ICSELECTION_DIRECTTI,
                .IC2Prescaler = TIM_ICPSC_DIV1,
                .IC2Filter = conf.filter,
            };
            while (HAL_TIM_Encoder_Init(htim, &sConfig) != HAL_OK);
            while (HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL) != HAL_OK);
        }

        /**
         * @brief measure the sensor
         *
         * do this periodically to update encoder state
         */
        void measure() {
            int16_t iCurrEnc = __HAL_TIM_GET_COUNTER(&tim.handle);
            uint32_t iCurrTick = HAL_GetTick();
            double dDiff = (int16_t) (iCurrEnc - iLastVal) * dFactor;
            dPosition += dDiff;
            dSpeed = dDiff / (iCurrTick - iLastTick) * 1000;
            iLastVal = iCurrEnc;
            iLastTick = iCurrTick;
        }

        /**
         * @brief return the counter value of the timer connected to the encoder
         * @return 16bit counter value
         */
        int16_t getValue() {
            return iLastVal;
        }

        /**
        * @brief return current value of encoder
        * with overflow correction
        * @return position, based on initialized factor
        */
        double getPosition() {
            return dPosition;
        }

        /**
         * @brief return current speed of encoder
         *
         * **disclaimer** this is just the numerically differentiated position
         * of the encoder.
         *
         * @return speed, based on initialized factor / s
         */
        double getSpeed() {
            return dSpeed;
        }

        /**
         * @brief reset encoder position
         */
        void zero() {
            __HAL_TIM_SET_COUNTER(&tim.handle, 0);
            iLastVal = 0;
            dPosition = 0;
            dSpeed = 0;
        }

    private:
        TIMER::HW tim;
        double dFactor;
        int16_t iLastVal = 0;
        uint32_t iLastTick = 0;

        double dPosition = 0;
        double dSpeed = 0;
    };
}
