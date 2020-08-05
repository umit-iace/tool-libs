/** @file encoder.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_ENCODER_H
#define STM_ENCODER_H
#include "stm/hal.h"
#include <assert.h>

class Encoder {
public:
    Encoder(uint32_t pinA, GPIO_TypeDef *portA, uint8_t alternateA,
            uint32_t pinB, GPIO_TypeDef *portB, uint8_t alternateB,
            TIM_TypeDef *tim, TIM_HandleTypeDef *handle)
            : handle(handle) {
        init_pins(pinA, portA, alternateA);
        init_pins(pinB, portB, alternateB);
        init(tim);
    }


    /**
     * @brief access the counter of the timer connected to the encoder
     * @return TimCounter
     */
    int16_t getPos() {
        return __HAL_TIM_GetCounter(handle);
    }

    /**
    * @brief access current value of encoder
    *        with overflow correction
    * @return position [rad]
    */
    double getPosRad() {
        int16_t currEnc = __HAL_TIM_GetCounter(handle);
        assert(this->dResolution != 0);
        dPosition += (double) (currEnc-lastEnc) * 2.0 * M_PI/this->dResolution;
        lastEnc = currEnc;
        return dPosition;
    }

    /**
    * @brief function to retrieve the current Resolution
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
    TIM_HandleTypeDef *handle = nullptr;

    void init(TIM_TypeDef *tim) {
        TIM_Encoder_InitTypeDef sConfig = {};
        TIM_MasterConfigTypeDef sMasterConfig = {};
        *handle = {};

        handle->Instance = tim;
        handle->Init.Prescaler = 0;
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        handle->Init.Period = 0xffff;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        handle->Init.RepetitionCounter = 0;
        handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
        sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
        sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
        sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
        sConfig.IC1Filter = 0;
        sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
        sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
        sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
        sConfig.IC2Filter = 0;
        while (HAL_TIM_Encoder_Init(handle, &sConfig) != HAL_OK);

        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        while (HAL_TIMEx_MasterConfigSynchronization(handle, &sMasterConfig) != HAL_OK);

        while (HAL_TIM_Encoder_Start(handle, TIM_CHANNEL_ALL) != HAL_OK);
    }

    void init_pins(uint32_t pin, GPIO_TypeDef *gpio, uint8_t alternate) {
        GPIO_InitTypeDef GPIO_InitStruct = {};
        GPIO_InitStruct.Pin = pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = alternate;
        HAL_GPIO_Init(gpio, &GPIO_InitStruct);
    }
private:
    int16_t lastEnc = 0;           ///< last Measurement of Encoder
    double dPosition = 0;          ///< stores current position in [rad]
    double dResolution = 2 * M_PI; ///< default Resolution
};
#endif //STM_ENCODER_H
