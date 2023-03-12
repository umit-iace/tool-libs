/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once

#include "stm/gpio.h"
#include "stm/registry.h"
#include "utils/Deadline.h"
#include "utils/Queue.h"
#include "x/Kern.h"

/** Character Buffer Sink & Source wrapping UART Peripheral 
 *
 * \dot
 * digraph HardwareUART_SINK_SOURCE {
 *      rankdir=LR;
 *      size="10"
 *      subgraph {
 *           rank=same;
 *           node [style=dashed, label="Buffer", URL="\ref Buffer"];
 *           SINK
 *           SRCE
 *      }
 *      HardwareUART [URL="\ref HardwareUART"];
 *      SINK -> HardwareUART [label="push", URL="\ref push"];
 *      HardwareUART -> SRCE [label="pop", URL="\ref pop"];
 *      }
 * \enddot
 */
struct HardwareUART : public Sink<Buffer<uint8_t>>, public Source<Buffer<uint8_t>> {
    /** default init struct */
    struct Default {
        USART_TypeDef *uart;
        AFIO rx, tx;
        uint32_t baudrate;
    };
    /** init struct with full control over UART settings */
    struct Manual {
        USART_TypeDef *uart;
        AFIO rx, tx;
        UART_InitTypeDef init;
    };
    /** constructor with default config */
    HardwareUART(const Default &conf);
    /** constructor with manually specifiable settings */
    HardwareUART(const Manual &conf);
    /** push bytebuffer into sending queue */
    void push(Buffer<uint8_t> &&tx) override;
    using Sink<Buffer<uint8_t>>::push;
    /** pull buffer from receiving queue */
    Buffer<uint8_t> pop() override;
    /** check if receiving queue is empty */
    bool empty() override;
    /** interrupt handler
     *
     * call this directly in interrupt
     */
    void irqHandler();
    /** stm glue registry for internal use */
    inline static Registry<HardwareUART, UART_HandleTypeDef, 8> reg;
    /** rx state */
    struct RX {
        Queue<Buffer<uint8_t>> q;
        Buffer<uint8_t> buf{512};
    } rx{};
    /** tx state */
    struct TX {
        Queue<Buffer<uint8_t>> q;
        Deadline deadline;
        bool active;
    } tx{};

    /** HAL handle for use with unwrapped UART capabilities */
    UART_HandleTypeDef handle{};
};
