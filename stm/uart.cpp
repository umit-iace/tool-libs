/** @file uart.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "uart.h"
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

/* General functions hidden from Interface */
void startTransmit(HardwareUART *uart) {
    if (uart->tx.q.empty()) return;
    uart->tx.active = true;
    auto &b = uart->tx.q.front();
    auto ret = HAL_OK;
    if ((ret = HAL_UART_Transmit_IT(&uart->handle, b.buf, b.len))) {
        if (ret != HAL_OK) {
            asm("bkpt");
        }
    }

    // need = ms/s * Bytes * (bit/Byte + Stop + Parity) / (bit/s)
    size_t need = 1000 * b.len * (8+2) / uart->handle.Init.BaudRate;
    uart->tx.timeout = Timeout{uwTick + need};
}
void startReceive(HardwareUART *uart) {
    auto ret = HAL_OK;
    if ((ret = HAL_UARTEx_ReceiveToIdle_IT(&uart->handle, uart->rx.buf.buf, uart->rx.buf.size))) {
        if (ret != HAL_OK) {
            asm("bkpt");
        }
    }
}

void _errcallback(UART_HandleTypeDef *handle) {
    auto uart = HardwareUART::reg.from(handle);
    HAL_UART_Abort(handle);
    startReceive(uart);
    startTransmit(uart);
}

void poll(HardwareUART *uart) {
    if (uart->tx.active && uart->tx.timeout(uwTick)) return _errcallback(&uart->handle);
    if (!uart->tx.active) return startTransmit(uart);
}

void _rxevent(UART_HandleTypeDef *handle, uint16_t nbs) {
    auto uart = HardwareUART::reg.from(handle);
    uart->rx.buf.len = nbs;
    if (!uart->rx.q.full()) {
        uart->rx.q.push(std::move(uart->rx.buf));
    } else {
        // dropping Buffer, reader should call more often...
    }
    uart->rx.buf = Buffer<uint8_t>{512};
    startReceive(uart);
}
void _rxcallback(UART_HandleTypeDef *handle) {
    auto uart = HardwareUART::reg.from(handle);
    _rxevent(handle, uart->rx.buf.size);
}
void _txcallback(UART_HandleTypeDef *handle) {
    auto uart = HardwareUART::reg.from(handle);
    auto& tx = uart->tx;
    tx.q.pop();
    tx.active = false;
    tx.timeout = Timeout{};
    poll(uart);
}
void init(HardwareUART *uart, UART_HandleTypeDef *handle) {
    HardwareUART::reg.reg(uart, handle);
    while (HAL_UART_Init(handle) != HAL_OK);
    HAL_UART_RegisterRxEventCallback(handle, _rxevent);
    HAL_UART_RegisterCallback(handle, HAL_UART_RX_COMPLETE_CB_ID, _rxcallback);
    HAL_UART_RegisterCallback(handle, HAL_UART_TX_COMPLETE_CB_ID, _txcallback);
    HAL_UART_RegisterCallback(handle, HAL_UART_ERROR_CB_ID, _errcallback);
    startReceive(uart);
}

// class methods
HardwareUART::HardwareUART(const Manual &conf) {
    handle = {
        .Instance = conf.uart,
        .Init = conf.init,
    };
    init(this, &handle);
}
HardwareUART::HardwareUART(const Default &conf) {
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
    init(this, &handle);
}

void HardwareUART::irqHandler() {
    HAL_UART_IRQHandler(&handle);
    /** workaround for HAL BUG: they still change state _AFTER_ calling */
    /** user callbacks (where we had already set it to what we needed) */
    /** luckily we only ever use this one type of reception */
    handle.ReceptionType = HAL_UART_RECEPTION_TOIDLE;
}
void HardwareUART::push(const Buffer<uint8_t> &b) {
    push(std::move(Buffer<uint8_t>{b}));
}
void HardwareUART::push(Buffer<uint8_t> &&b) {
    tx.q.push(std::move(b));
    poll(this);
}
bool HardwareUART::empty() {
    return rx.q.empty();
}
Buffer<uint8_t> HardwareUART::pop() {
    return rx.q.pop();
}
