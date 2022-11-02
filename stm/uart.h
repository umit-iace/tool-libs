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
    unsigned char buf[SZ]{};
    unsigned short len{};
    void (*cb)(){};
    UART_TX(unsigned char *src=nullptr,
            unsigned short len=0,
            void (*cb)()=nullptr) : len(len), cb(cb) {
        memcpy(buf, src, len);
    }
};
struct UART_RX {
    UART_RX(unsigned char *dst,
            unsigned short len,
            void (*cb)()=nullptr) : buf(dst), len(len), cb(cb) {}
    unsigned char *buf{};
    unsigned short len{};
    void (*cb)(){};
};
/**
 * @brief Template class for hardware based UART derivations
 *
 * API:
 * *tx: use the @ref request(UART_TX) method to set up sending a buffer
 *      over the serial line
 * *rx:
 *      - use the @ref request(UART_RX) method if exact length to be
 *      received is known
 *      - use the @ref listen(void (*func)(char c)) method to set up a
 *      listener on this interface. All received bytes will be sent to it
 */
class HardwareUART : public RequestQueue<UART_TX>, public RequestQueue<UART_RX>{
public:
    inline static Registry<HardwareUART, UART_HandleTypeDef, 8> reg;
    void (*listener)(uint8_t c){};
    uint8_t listenbuf[512];
    /**
     * Initialize the peripheral
     *
     * make sure to also initialize the corresponding rx/tx AFIO pins
     *
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     */
    HardwareUART(USART_TypeDef *dUsart, uint32_t iBaudRate);
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
    short request(UART_TX *r) override;
    short request(UART_RX *r) override;
    void rqBegin(UART_TX *r) override;
    void rqBegin(UART_RX *r) override;
    void rqTimeout(UART_TX *r) override;
    void rqTimeout(UART_RX *r) override;
    unsigned long getTime() override;
    /* listener api */
    void listen(void (*func)(uint8_t c));
};
#endif //STM_UART_H
