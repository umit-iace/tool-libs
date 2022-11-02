/** @file uart.cpp
 *
 * Copyright (c) 2022 IACE
 */
#include "uart.h"
#include "stm/hal.h"
#include "stm/gpio.h"

/* HAL glue */
void _rxcallback(UART_HandleTypeDef *handle, uint16_t nbs) {
    HardwareUART::reg.from(handle)->rxcallback(nbs);
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
    HAL_UART_RegisterRxEventCallback(&handle, _rxcallback);
    HAL_UART_RegisterCallback(&handle, HAL_UART_TX_COMPLETE_CB_ID, _txcallback);
    HAL_UART_RegisterCallback(&handle, HAL_UART_ERROR_CB_ID, _errcallback);
}

void HardwareUART::rxcallback(uint16_t nbs) {
    bool rqend = false;
    uint8_t c= nbs+'0';
    uint8_t nl = '\n';
    request(new UART_TX(&nl, 1));
    request(new UART_TX(&c, 1));
    request(new UART_TX(&nl, 1));
    auto r = RequestQueue<UART_RX>::rqCurrent();
    r->buf += nbs;
    r->len -= nbs;
    /* if (r->buf + r->len == handle.) { */
    if (handle.RxXferCount == 0) {
        rqend = true;
        auto led = DIO(GPIO_PIN_13, GPIOC);
        led.set(false);
    } else {
        assert(r->len);
        rqBegin(r);
    }
    if (r->bytewise) {
        do {
            r->bytewise(r->buf[-(--nbs+1)]);
        } while (nbs);
    }
    if (rqend) {
        RequestQueue<UART_RX>::rqEnd();
    }
}
void HardwareUART::txcallback() {
    this->RequestQueue<UART_TX>::rqEnd();
}

short HardwareUART::request(UART_TX *r) {
    return this->RequestQueue<UART_TX>::request(r);
}
short HardwareUART::request(UART_RX *r) {
    return this->RequestQueue<UART_RX>::request(r);
}

void HardwareUART::irqHandler() {
    HAL_UART_IRQHandler(&handle);
}

void HardwareUART::rqBegin(UART_TX *r) {
    HAL_UART_Transmit_IT(&handle, r->buf, r->len);
}
void HardwareUART::rqBegin(UART_RX *r) {
    HAL_UARTEx_ReceiveToIdle_IT(&handle, r->buf, r->len);
}
/* we don't use these */
void HardwareUART::rqTimeout(UART_TX *r) { /* not implemented */ }
void HardwareUART::rqTimeout(UART_RX *r) { /* not implemented */ }
unsigned long HardwareUART::getTime() { return 0; }
