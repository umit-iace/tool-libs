/** @file encoder.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_ENCODER_H
#define STM_ENCODER_H
#include "stm/hal.h"

class Encoder {
public:
    Encoder(uint32_t pinA, GPIO_TypeDef *portA, uint8_t alternateA,
            uint32_t pinB, GPIO_TypeDef *portB, uint8_t alternateB,
            TIM_HandleTypeDef *hTim)
            : hTim(hTim) {
        init();
        initPins(pinA, portA, alternateA);
        initPins(pinB, portB, alternateB);
    }


    /**
     * @brief access the counter of the timer connected to the encoder
     * @return TimCounter
     */
    int16_t getPos() {
        return __HAL_TIM_GetCounter(hTim);
    }

    /**
    * @brief access current value of encoder
    *        with overflow correction
    * @return position [rad]
    */
    double getPosRad() {
        int16_t iCurrEnc = this->getPos();
        int16_t iDiffEnc = iCurrEnc - iLastEnc;
        dPosition += iDiffEnc * 2.0 * M_PI / this->dResolution;
        iLastEnc = iCurrEnc;
        return dPosition;
    }

    /**
    * @brief access current value of encoder
    *        with overflow correction
    * @return position [rad]
    */
    double getPosDeg() {
        int16_t iCurrEnc = this->getPos();
        int16_t iDiffEnc = iCurrEnc - iLastEnc;
        dPosition += iDiffEnc * 360 / this->dResolution;
        iLastEnc = iCurrEnc;
        return dPosition;
    }

    /**
    * @brief function to retrieve the current resolution
    * @return dResolution
    */
    double getResolution(){
        return this->dResolution;
    }

    /**
     * @brief sets the parameter dResoltion
     * @param newResolution
     */
    void setResolution(double newResolution){
        if(newResolution != 0)
            this->dResolution = newResolution;
    }

private:
    void init() {
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

    void initPins(uint32_t pin, GPIO_TypeDef *gpio, uint8_t alternate) {
        GPIO_InitTypeDef GPIO_InitStruct = {};
        GPIO_InitStruct.Pin = pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = alternate;
        HAL_GPIO_Init(gpio, &GPIO_InitStruct);
    }
private:
    TIM_HandleTypeDef *hTim = nullptr;

    int16_t iLastEnc = 0;              ///< last measurement
    double dPosition = 0;              ///< stores current position in [rad]
    double dResolution = 2 * M_PI;     ///< default resolution
};
#endif //STM_ENCODER_H
