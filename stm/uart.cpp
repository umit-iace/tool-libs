/** @file uart.cpp
 *
 * Copyright (c) 2022 IACE
 */
#include "uart.h"
#include "stm/hal.h"
#include "stm/gpio.h"

/* HAL glue */
void _rxevent(UART_HandleTypeDef *handle, uint16_t nbs) {
    HardwareUART::reg.from(handle)->rxevent(nbs);
}
void _rxcallback(UART_HandleTypeDef *handle) {
    HardwareUART::reg.from(handle)->rxcallback();
}
void _txcallback(UART_HandleTypeDef *handle) {
    HardwareUART::reg.from(handle)->txcallback();
}
void _errcallback(UART_HandleTypeDef *handle) {
    auto led = DIO(GPIO_PIN_13, GPIOC);
    led.set(true);
}

/**
 * Initialize the peripheral
 *
 * make sure to also initialize the corresponding rx/tx AFIO pins
 *
 * @param dUsart definition of used UART
 * @param iBaudRate baud rate
 */
HardwareUART::HardwareUART(USART_TypeDef *dUsart, uint32_t iBaudRate) :
    RequestQueue<UART_TX>(30, 0),
    RequestQueue<UART_RX>(10, 0)
{
    reg.reg(this, &handle);
    handle = {
        .Instance = dUsart,
        .Init = {
            .BaudRate = iBaudRate,
            .WordLength = UART_WORDLENGTH_8B,
            .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE,
            .Mode = UART_MODE_TX_RX,
            .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
    };
    while (HAL_UART_Init(&handle) != HAL_OK);
    HAL_UART_RegisterRxEventCallback(&handle, _rxevent);
    HAL_UART_RegisterCallback(&handle, HAL_UART_RX_COMPLETE_CB_ID, _rxcallback);
    HAL_UART_RegisterCallback(&handle, HAL_UART_TX_COMPLETE_CB_ID, _txcallback);
    HAL_UART_RegisterCallback(&handle, HAL_UART_ERROR_CB_ID, _errcallback);
}

void HardwareUART::rxevent(uint16_t nbs) {
    uint16_t workleft = handle.RxXferSize - nbs;
    uint8_t *p = handle.pRxBuffPtr;
    if (workleft < 64) {
        workleft = sizeof listenbuf;
        p = listenbuf;
    }
    uint8_t *tmp = handle.pRxBuffPtr - nbs;
    HAL_UARTEx_ReceiveToIdle_IT(&handle, p, workleft);
    for (uint16_t i = 0; i < nbs; ++i) {
        listener(tmp[i]);
    }
}
void HardwareUART::rxcallback() {
    auto r = RequestQueue<UART_RX>::rqCurrent();
    if (r->cb) r->cb();
    HAL_UART_AbortReceive_IT(&handle);
    RequestQueue<UART_RX>::rqEnd();
}
void HardwareUART::txcallback() {
    auto r = RequestQueue<UART_TX>::rqCurrent();
    if (r->cb) r->cb();
    RequestQueue<UART_TX>::rqEnd();
}

short HardwareUART::request(UART_TX *r) {
    return RequestQueue<UART_TX>::request(r);
}
short HardwareUART::request(UART_RX *r) {
    return RequestQueue<UART_RX>::request(r);
}

void HardwareUART::irqHandler() {
    HAL_UART_IRQHandler(&handle);
}

void HardwareUART::rqBegin(UART_TX *r) {
    auto ret = HAL_UART_Transmit_IT(&handle, r->buf, r->len);
    if (ret != HAL_OK) {
        asm("bkpt");
    }

}
void HardwareUART::rqBegin(UART_RX *r) {
    auto ret = HAL_UART_Receive_IT(&handle, r->buf, r->len);
    if (ret != HAL_OK) {
        asm("bkpt");
    }
}
/* we don't use these */
void HardwareUART::rqTimeout(UART_TX *r) { /* not implemented */ }
void HardwareUART::rqTimeout(UART_RX *r) { /* not implemented */ }
unsigned long HardwareUART::getTime() { return 0; }
void HardwareUART::listen(void (*func)(uint8_t c)) {
    assert(listener == nullptr);
    listener = func;
    HAL_UARTEx_ReceiveToIdle_IT(&handle, listenbuf, sizeof listenbuf);
}
