/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_UART_H
#define STM_UART_H

#include "stm/hal.h"
#include "stm/registry.h"
#include "utils/RequestQueue.h"
#include <cstring>

struct UART_TX {
    static constexpr unsigned int SZ = 512;
    unsigned char buf[SZ];
    unsigned short len;
    UART_TX() {len = 0;}
    UART_TX(unsigned char *src, unsigned short len) : len(len) {
        memcpy(buf, src, len);
    }
};
struct UART_RX {
    UART_RX() {buf = nullptr; len = 0;}
    UART_RX(unsigned char *dst, unsigned short len) : buf(dst), len(len) {}
    UART_RX(unsigned char *dst,
            unsigned short len,
            void (*bw)(uint8_t c)) : buf(dst), len(len), bytewise(bw) {}
    unsigned char *buf;
    unsigned short len;
    void (*bytewise)(uint8_t c) = nullptr;
};
/**
 * @brief Template class for hardware based UART derivations
 */
class HardwareUART : public RequestQueue<UART_TX>, public RequestQueue<UART_RX>{
public:
    inline static Registry<HardwareUART, UART_HandleTypeDef, 8> reg;
    /**
     * Initialize the peripheral
     *
     * make sure to also initialize the corresponding rx/tx AFIO pins
     *
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     */
    HardwareUART(USART_TypeDef *dUsart, uint32_t iBaudRate);
    void rxcallback(uint16_t nbs);
    void txcallback();
    /** interrupt handler
     *
     * call this directly in interrupt
     */
    void irqHandler();
    /** HAL handle for use with unwrapped UART capabilities */
    UART_HandleTypeDef handle{};
    /* queue defs */
    short request(UART_TX *r) override;
    short request(UART_RX *r) override;
    void rqBegin(UART_TX *r) override;
    void rqBegin(UART_RX *r) override;
    void rqTimeout(UART_TX *r) override;
    void rqTimeout(UART_RX *r) override;
    unsigned long getTime() override;
};
#endif //STM_UART_H
