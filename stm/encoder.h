/** @file encoder.h
 *
 * Copyright (c) 2020 IACE
 */

#ifndef STM_ENCODER_H
#define STM_ENCODER_H

#include "stm/gpio.h"
#include "stm/hal.h"

class Encoder {
public:
    /**
     * Encoder constructor.
     *
     * make sure the pins for the encoder are on
     * channels 1 and 2 of the used timer.
     *
     * @param pinA pin number of encoder A pin
     * @param portA port of encoder A pin
     * @param alternateA alternate function of encoder A pin
     * @param pinB pin number of encoder B pin
     * @param portB port of encoder B pin
     * @param alternateB alternate function of encoder B pin
     * @param hTim timer handle
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
    Encoder(uint32_t pinA, GPIO_TypeDef *portA, uint8_t alternateA,
            uint32_t pinB, GPIO_TypeDef *portB, uint8_t alternateB,
            TIM_HandleTypeDef *hTim, double dFactor)
            : hTim(hTim), dFactor(dFactor) {
        AFIO(pinA, portA, alternateA);
        AFIO(pinB, portB, alternateB);

        TIM_Encoder_InitTypeDef sConfig = {};
        sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
        sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
        sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
        sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
        sConfig.IC1Filter = 0;
        sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
        sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
        sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
        sConfig.IC2Filter = 0;
        while (HAL_TIM_Encoder_Init(hTim, &sConfig) != HAL_OK);
        while (HAL_TIM_Encoder_Start(hTim, TIM_CHANNEL_ALL) != HAL_OK);
    }

    /**
     * @brief measure the sensor
     *
     * do this periodically to update encoder state
     */
    void measure() {
        int16_t iCurrEnc = __HAL_TIM_GET_COUNTER(hTim);
        uint32_t iCurrTick = HAL_GetTick();
        double dDiff = (int16_t)(iCurrEnc - iLastVal) * dFactor;
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
        __HAL_TIM_SET_COUNTER(hTim, 0);
        iLastVal = 0;
        dPosition = 0;
    }

private:
    ///\cond false
    TIM_HandleTypeDef *hTim = nullptr;
    double dFactor = 1;
    int16_t iLastVal = 0;
    uint32_t iLastTick = 0;

    double dPosition = 0;
    double dSpeed = 0;
    ///\endcond
};

#endif //STM_ENCODER_H
