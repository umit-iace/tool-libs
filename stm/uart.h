/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_UART_H
#define STM_UART_H

#include "stm/hal.h"

/**
 * @brief Template class for hardware based UART derivations
 */
class HardwareUART {
public:
    /**
     * Initialize the peripheral
     *
     * make sure to also initialize the corresponding rx/tx AFIO pins
     *
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     */
    HardwareUART(USART_TypeDef *dUsart, uint32_t iBaudRate) {
        handle.Instance = dUsart;
        handle.Init = {
                .BaudRate = iBaudRate,
                .WordLength = UART_WORDLENGTH_8B,
                .StopBits = UART_STOPBITS_1,
                .Parity = UART_PARITY_NONE,
                .Mode = UART_MODE_TX_RX,
                .HwFlowCtl = UART_HWCONTROL_NONE,
                .OverSampling = UART_OVERSAMPLING_16,
        };
        while (HAL_UART_Init(&handle) != HAL_OK);
    }

    UART_HandleTypeDef handle{};
};

#endif //STM_UART_H
