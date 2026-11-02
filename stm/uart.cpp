/** @file uart.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "sys/uart.h"
using namespace UART;
namespace debug {
    namespace uart {
// debugging helpers
struct SR {
    uint32_t PE:1;
    uint32_t FE:1;
    uint32_t NF:1;
    uint32_t ORE:1;
    uint32_t IDLE:1;
    uint32_t RXNE:1;
    uint32_t TC:1;
    uint32_t TXE:1;
    uint32_t LBD:1;
    uint32_t CTS:1;
    uint32_t _:22;
};
struct DR {
    uint32_t DR:9;
    uint32_t _:23;
};
struct BRR {
    uint32_t DIV_Fraction:4;
    uint32_t DIV_Mantissa:12;
    uint32_t _:16;
};
struct CR1 {
    uint32_t SBK:1;
    uint32_t RWU:1;
    uint32_t RE:1;
    uint32_t TE:1;
    uint32_t IDLEIE:1;
    uint32_t RXNEIE:1;
    uint32_t TCIE:1;
    uint32_t TXEIE:1;
    uint32_t PEIE:1;
    uint32_t PS:1;
    uint32_t PCE:1;
    uint32_t WAKE:1;
    uint32_t M:1;
    uint32_t UE:1;
    uint32_t _:1;
    uint32_t OVER8:1;
    uint32_t _1:16;
};
struct CR2 {
    uint32_t ADD:4;
    uint32_t _:1;
    uint32_t LBDL:1;
    uint32_t LBDIE:1;
    uint32_t _1:1;
    uint32_t LBCL:1;
    uint32_t CPHA:1;
    uint32_t CPOL:1;
    uint32_t CLKEN:1;
    uint32_t STOP:2;
    uint32_t LINEN:1;
    uint32_t _2:17;
};
struct CR3 {
    uint32_t EIE:1;
    uint32_t IREN:1;
    uint32_t IRLP:1;
    uint32_t HDSEL:1;
    uint32_t NACK:1;
    uint32_t SCEN:1;
    uint32_t DMAR:1;
    uint32_t DMAT:1;
    uint32_t RTSE:1;
    uint32_t CTSE:1;
    uint32_t CTSIE:1;
    uint32_t ONEBIT:1;
    uint32_t _:20;
};
struct GTPR {
    uint32_t PSC:8;
    uint32_t GT:8;
    uint32_t _:16;
};
struct UART_INST {
    struct SR SR;
    struct DR DR;
    struct BRR BRR;
    struct CR1 CR1;
    struct CR2 CR2;
    struct CR3 CR3;
    struct GTPR GTPR;
} inst;
}}

/* General functions hidden from Interface */
void startTransmit(HW *uart) {
    if (uart->tx.q.empty()) return;
    auto &b = uart->tx.q.front();
    if (HAL_UART_Transmit_IT(&uart->handle, b.buf, b.len) != HAL_OK) {
        // e.g. still busy: HW::service tries again
        ++uart->tx.errors;
        uart->tx.active = false;
        return;
    }
    uart->tx.active = true;

    // need = ms/s * Bytes * (bit/Byte + Stop + Parity) / (bit/s), rounded up
    // + 2ms margin, so transfers don't time out on tick granularity
    uint32_t baud = uart->handle.Init.BaudRate;
    uint32_t need = (1000ull * b.len * (8+2) + baud - 1) / baud + 2;
    uart->tx.deadline = Deadline::after(uwTick, need);
}
/** the DMA writes the ring behind the CPU's back */
void invalidate(HW::RXDMA *d) {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_InvalidateDCache_by_Addr((uint32_t *)d->ring.b, sizeof d->ring.b);
#else
    (void)d;
#endif
}
void startReceive(HW *uart) {
    if (auto d = uart->rxDma) {
        // circular: runs on its own, the main loop collects the bytes, see fetch
        invalidate(d);
        uart->rx.running = HAL_UART_Receive_DMA(&uart->handle,
                d->ring.b, HW::RINGSIZE) == HAL_OK;
        if (uart->rx.running) d->rd = 0;
        // the HAL completes an abort after an error in the TC interrupt, keep that one
        __HAL_DMA_DISABLE_IT(&d->dma.handle, DMA_IT_HT);
    } else {
        uart->rx.running = HAL_UARTEx_ReceiveToIdle_IT(&uart->handle,
                uart->rx.buf.buf, uart->rx.buf.size) == HAL_OK;
    }
    // HW::service tries again
    if (!uart->rx.running) ++uart->rx.errors;
}
/** queue what the DMA received since the last call, runs in the main loop
 *
 * shortcut: a ring overrun (main loop stalled for RINGSIZE bytes) is not detected,
 * MIN drops the broken frames; count laps in the DMA interrupts if that matters */
void fetch(HW *uart) {
    auto d = uart->rxDma;
    size_t wr = HW::RINGSIZE - __HAL_DMA_GET_COUNTER(&d->dma.handle);
    if (wr == HW::RINGSIZE) wr = 0;
    if (wr == d->rd || uart->rx.q.full()) return;
    size_t n = (wr + HW::RINGSIZE - d->rd) % HW::RINGSIZE;
    invalidate(d);
    Buffer<uint8_t> b(n);
    for (size_t i = 0; i < n; ++i) {
        b.append(d->ring.b[(d->rd + i) % HW::RINGSIZE]);
    }
    d->rd = wr;
    uart->rx.q.push(std::move(b));
}

/** HAL errors (overrun, framing, noise, parity) are all on the receiving side,
 * a running transmission is left alone */
void deliver(HW *uart, uint16_t nbs);
void _errcallback(UART_HandleTypeDef *handle) {
    auto uart = HW::reg.from(handle);
    if (!uart) return;
    if (uart->rxDma) {
        // framing & noise errors leave the DMA running, MIN drops the broken frame;
        // after an overrun the HAL stops it, HW::service restarts it after fetching
        if (handle->RxState != HAL_UART_STATE_BUSY_RX) uart->rx.running = false;
        return;
    }
    // hand on what arrived before the error, only the broken byte is lost
    uint16_t got = handle->RxXferSize - handle->RxXferCount;
    HAL_UART_AbortReceive(handle);
    if (got && got <= uart->rx.buf.size) deliver(uart, got);
    startReceive(uart);
}
/** retry a transmission that didn't finish in time, leaves reception alone */
void _txtimeout(HW *uart) {
    HAL_UART_AbortTransmit(&uart->handle);
    startTransmit(uart);
}

void poll(HW *uart) {
    if (uart->tx.active && uart->tx.deadline(uwTick)) return _txtimeout(uart);
    if (!uart->tx.active) return startTransmit(uart);
}

/** queue the nbs bytes received into rx.buf, continue with a fresh buffer
 *
 * runs in the interrupt: takes a spare buffer, allocates only if none left
 */
void deliver(HW *uart, uint16_t nbs) {
    auto &rx = uart->rx;
    if (rx.q.full()) {
        // dropping the data, reader should call more often...
        // the buffer is reused, nothing to free in here
        rx.buf.len = 0;
        return;
    }
    rx.buf.len = nbs;
    rx.q.push(std::move(rx.buf));
    if (!rx.spare.empty()) {
        rx.buf = rx.spare.pop();
    } else {
        rx.buf = Buffer<uint8_t>(HW::RXSIZE);
        ++rx.allocs;
    }
}
void _rxevent(UART_HandleTypeDef *handle, uint16_t nbs) {
    auto uart = HW::reg.from(handle);
    if (!uart) return;
    deliver(uart, nbs);
    startReceive(uart);
}
void _rxcallback(UART_HandleTypeDef *handle) {
    auto uart = HW::reg.from(handle);
    // DMA: a lap of the ring, the bytes are collected by fetch
    if (!uart || uart->rxDma) return;
    _rxevent(handle, uart->rx.buf.size);
}
void _txcallback(UART_HandleTypeDef *handle) {
    auto uart = HW::reg.from(handle);
    if (!uart) return;
    auto& tx = uart->tx;
    tx.q.drop();
    tx.active = false;
    tx.deadline = Deadline{};
    poll(uart);
}
void init(HW *uart, UART_HandleTypeDef *handle, DMA_Stream_TypeDef *rxDmaStream,
          uint32_t rxDmaChannel) {
    halInit();
    k.every(1, *uart, &HW::service);
    HW::reg.reg(uart, handle);
    while (HAL_UART_Init(handle) != HAL_OK);
    if (rxDmaStream) {
        uart->rxDma = new HW::RXDMA{{{
            .stream = rxDmaStream,
            .init = {
                .Channel = rxDmaChannel,
                .Direction = DMA_PERIPH_TO_MEMORY,
                .MemInc = DMA_MINC_ENABLE,
                .PeriphDataAlignment = DMA_PDATAALIGN_BYTE,
                .MemDataAlignment = DMA_MDATAALIGN_BYTE,
                .Mode = DMA_CIRCULAR,
                .Priority = DMA_PRIORITY_HIGH,
            },
        }}};
        __HAL_LINKDMA(handle, hdmarx, uart->rxDma->dma.handle);
    }
    HAL_UART_RegisterRxEventCallback(handle, _rxevent);
    HAL_UART_RegisterCallback(handle, HAL_UART_RX_COMPLETE_CB_ID, _rxcallback);
    HAL_UART_RegisterCallback(handle, HAL_UART_TX_COMPLETE_CB_ID, _txcallback);
    HAL_UART_RegisterCallback(handle, HAL_UART_ERROR_CB_ID, _errcallback);
    startReceive(uart);
}

// class methods
HW::HW(const Manual &conf) {
    handle = {
        .Instance = conf.uart,
        .Init = conf.init,
    };
    init(this, &handle, conf.rxDmaStream, conf.rxDmaChannel);
}
HW::HW(const Default &conf) {
    handle = {
        .Instance = conf.uart,
        .Init = {
            .BaudRate = conf.baudrate,
            .WordLength = UART_WORDLENGTH_8B,
            .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE,
            .Mode = UART_MODE_TX_RX,
            .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
    };
    init(this, &handle, conf.rxDmaStream, conf.rxDmaChannel);
}

void HW::irqHandler() {
    HAL_UART_IRQHandler(&handle);
    /* workaround for HAL BUG: they still change state _AFTER_ calling */
    /* user callbacks (where we had already set it to what we needed) */
    /* luckily we only ever use this one type of reception */
    if (!rxDma) handle.ReceptionType = HAL_UART_RECEPTION_TOIDLE;
}
void HW::rxDmaIrqHandler() {
    if (rxDma) HAL_DMA_IRQHandler(&rxDma->dma.handle);
}
bool HW::full() {
    return tx.q.full();
}
void HW::push(Buffer<uint8_t> &&b) {
    // tx state is shared with the interrupt handlers
    CriticalSection cs;
    tx.q.push(std::move(b));
    poll(this);
}
bool HW::empty() {
    if (rxDma) fetch(this);
    return rx.q.empty();
}
void HW::service(uint32_t, uint32_t) {
    // allocate here, outside the interrupt and the CriticalSection
    while (!rxDma && rx.spare.size() < rx.spare.capacity()) {
        Buffer<uint8_t> b(RXSIZE);
        CriticalSection cs;
        rx.spare.push(std::move(b));
    }
    // the bytes up to an overrun are still in the ring
    if (rxDma) fetch(this);
    CriticalSection cs;
    if (!rx.running) startReceive(this);
    poll(this);
}
Buffer<uint8_t> HW::pop() {
    CriticalSection cs;
    return rx.q.pop();
}
