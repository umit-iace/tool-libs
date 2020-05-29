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
            TIM_TypeDef *tim, TIM_HandleTypeDef *handle) : handle(handle) {
        init_pins(pinA, portA, alternateA);
        init_pins(pinB, portB, alternateB);
        init(tim);
    }

    /**
     * return current value of encoder
     * @todo calculate directly in rad?
     * @return
     */
    int16_t getPos() {
        return __HAL_TIM_GetCounter(handle);
    }

    /**
     *  @brief returns value of encode in rad
     *  @return encoder position in [rad]
     */
    double getPosRad(){
        return ((this->getPos()*2.0*M_PI)/this->dResolution);
    }
private:
    TIM_HandleTypeDef *handle = nullptr;
    double dResolution;

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
};
#endif //STM_ENCODER_H
