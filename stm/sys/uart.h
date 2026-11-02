/** @file uart.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once
#include <utils/deadline.h>
#include <utils/queue.h>
#include <core/kern.h>
#include <core/critical.h>

#include "dma.h"
#include "gpio.h"
#include "registry.h"

namespace UART {
/** Character Buffer Sink & Source wrapping UART Peripheral
 *
 * \dot
 * digraph UART_SINK_SOURCE {
 *      rankdir=LR;
 *      size="10"
 *      subgraph {
 *           rank=same;
 *           node [style=dashed, label="Buffer", URL="\ref Buffer"];
 *           SINK
 *           SRCE
 *      }
 *      UART [URL="\ref HW"];
 *      SINK -> UART [label="push", URL="\ref push"];
 *      UART -> SRCE [label="pop", URL="\ref pop"];
 *      }
 * \enddot
 */
struct HW : public Sink<Buffer<uint8_t>>, public Source<Buffer<uint8_t>> {
    /** default init struct */
    struct Default {
        USART_TypeDef *uart; ///< UART peripheral
        AFIO rx, tx; ///< alternate function initialized pins
        uint32_t baudrate; ///< baudrate
        /** optional: receive by DMA instead of an interrupt per byte, needed for high
         * baudrates (921600: a byte every 10.9 us). Enable the clock of the DMA before
         * and call \ref rxDmaIrqHandler in the interrupt of the stream */
        DMA_Stream_TypeDef *rxDmaStream{};
        uint32_t rxDmaChannel{}; ///< DMA channel of the UART's RX request
    };
    /** init struct with full control over UART settings */
    struct Manual {
        USART_TypeDef *uart; ///< UART peripheral
        AFIO rx, tx; ///< alternate function initialized pins
        UART_InitTypeDef init; ///< HAL Init configuration
        DMA_Stream_TypeDef *rxDmaStream{}; ///< see Default
        uint32_t rxDmaChannel{}; ///< see Default
    };
    /** constructor with default config */
    HW(const Default &conf);
    /** constructor with manually specifiable settings */
    HW(const Manual &conf);
    bool full() override;
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
    /** DMA stream interrupt handler of the reception, call it directly in interrupt */
    void rxDmaIrqHandler();
    /** stm glue registry for internal use */
    inline static Registry<HW, UART_HandleTypeDef, 8> reg;
    /** size of the receive buffers */
    static constexpr size_t RXSIZE = 512;
    /** rx state */
    struct RX {
        Queue<Buffer<uint8_t>> q;
        Buffer<uint8_t> buf = Buffer<uint8_t>(RXSIZE);
        /** allocated in the main loop, the interrupt takes them */
        Queue<Buffer<uint8_t>> spare{4};
        bool running; ///< reception started
        uint32_t errors; ///< reception the HAL refused to start
        uint32_t allocs; ///< buffers allocated in the interrupt (no spare)
    } rx{};
    /** [B] ring the DMA receives into, whole cache lines (Cortex-M7: 32 bytes)
     *
     * emptied by the main loop, at 921600 baud it holds 11 ms */
    static constexpr size_t RINGSIZE = 1024;
    /** DMA reception, nullptr: interrupt per byte */
    struct RXDMA {
        DMA::HW dma;
        struct alignas(32) {
            uint8_t b[RINGSIZE];
        } ring{};
        size_t rd{}; ///< position up to which the ring was read
    } *rxDma{};
    /** tx state */
    struct TX {
        Queue<Buffer<uint8_t>> q;
        Deadline deadline;
        bool active;
        uint32_t errors; ///< transmissions the HAL refused to start
    } tx{};
    /** main loop housekeeping, registered with the kernel: retries
     * refused starts, provides spare receive buffers */
    void service(uint32_t, uint32_t);

    /** HAL handle for use with unwrapped UART capabilities */
    UART_HandleTypeDef handle{};
};
}
