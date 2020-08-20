/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_UART_H
#define STM_UART_H

#include "stm/gpio.h"
#include "stm/hal.h"

/**
 * @brief Template class for hardware based UART derivations
 */
class HardwareUART {
public:
    /**
     * Constructor, that initialize the RX and TX pins and configure UART instance
     * @param iRXPin RX pin address
     * @param gpioRXPort definition of RX port
     * @param iRXAlternate address of alternate RX pin functionality
     * @param iTXPin TX pin address
     * @param gpioTXPort definition of TX port
     * @param iTXAlternate address of alternate TX pin functionality
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     * @param hUart uart handle
     */
    HardwareUART(uint32_t iRXPin, GPIO_TypeDef *gpioRXPort, uint8_t iRXAlternate,
                 uint32_t iTXPin, GPIO_TypeDef *gpioTXPort, uint8_t iTXAlternate,
                 USART_TypeDef *dUsart, uint32_t iBaudRate, UART_HandleTypeDef *hUart) :
                    dUsart(dUsart), iBaudRate(iBaudRate), hUart(hUart) {
        AFIO(iRXPin, gpioRXPort, iRXAlternate);
        AFIO(iTXPin, gpioTXPort, iTXAlternate);

        this->config();
    }

    //\cond false
    UART_HandleTypeDef *hUart;
    //\endcond

private:
    //\cond false
    USART_TypeDef *dUsart;

    uint32_t iBaudRate;

    void config() {
        *this->hUart = {};
        this->hUart->Instance = this->dUsart;
        this->hUart->Init.BaudRate = this->iBaudRate;
        this->hUart->Init.WordLength = UART_WORDLENGTH_8B;
        this->hUart->Init.StopBits = UART_STOPBITS_1;
        this->hUart->Init.Parity = UART_PARITY_NONE;
        this->hUart->Init.Mode = UART_MODE_TX_RX;
        this->hUart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
        this->hUart->Init.OverSampling = UART_OVERSAMPLING_16;
        while (HAL_UART_Init(this->hUart) != HAL_OK);
    };
    //\endcond
};

#endif //STM_UART_H
