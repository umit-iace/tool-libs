/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once

#include "stm/gpio.h"
#include "stm/hal.h"
#include "stm/registry.h"
#include "utils/RequestQueue.h"
#include "utils/Buffer.h"
#include "hw/uart.h"

class HardwareUART : public RequestQueue<Buffer<uint8_t>>{
public:
    using UART_TX = Buffer<uint8_t>;
    inline static Registry<HardwareUART, UART_HandleTypeDef, 8> reg;
    uint8_t listenbuf[512];
    ByteHandler *listener;
    /**
     * Initialize the peripheral
     *
     * make sure to also initialize the corresponding rx/tx AFIO pins
     *
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     */
    HardwareUART(USART_TypeDef *dUsart, uint32_t iBaudRate, ByteHandler *l);
    void rxevent(uint16_t nbs);
    void rxcallback();
    void txcallback();
    /** interrupt handler
     *
     * call this directly in interrupt
     */
    void irqHandler();
    /** HAL handle for use with unwrapped UART capabilities */
    UART_HandleTypeDef handle{};
    /* queue defs */
    /* short request(UART_TX *r) override; */
    void rqBegin(UART_TX *r) override;
    void rqTimeout(UART_TX *r) override;
    unsigned long getTime() override;
};

struct UART: RequestQueue<UARTTxRequest> {
    struct Basic {
        USART_TypeDef *uart;
        AFIO rx, tx;
        UART_InitTypeDef init;
        ByteHandler *handler;
        size_t qlen;
        size_t timeout;
    };
    UART(Basic b)
            : RequestQueue<UARTTxRequest>(b.qlen, b.timeout)
            , handler(b.handler)
    {
        handle.Instance = b.uart;
        handle.Init = b.init;
        while(HAL_UART_Init(&handle) != HAL_OK);
        /* HAL_UART_Receive */
    }
    UART_HandleTypeDef handle{};
    ByteHandler *handler{};

    void txcallback() {
        rqEnd();
    }
    void rqBegin(UARTTxRequest *r) override {
        HAL_UART_Transmit_IT(&handle, r->payload, r->len);
    }
    void rqTimeout(UARTTxRequest *r) override {
    }
    unsigned long getTime() override {
        return 0;
    }
    void irqHandler() {
        HAL_UART_IRQHandler(&handle);
    }
};
