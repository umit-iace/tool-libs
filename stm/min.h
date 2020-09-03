/** @file min.h
 *
 * Copyright (c) 2020 IACE
 */

#ifndef STMMIN_H
#define STMMIN_H

#include "define.h"
#include "utils/Min.h"
#include "stm/hal.h"
#include "stm/uart.h"


class StmMin : public MinImpl, public HardwareUART {
public:
    StmMin(): HardwareUART(TRANSPORT_RX_PIN, TRANSPORT_RX_PORT, TRANSPORT_RX_ALTERNATE,
                         TRANSPORT_TX_PIN, TRANSPORT_TX_PORT, TRANSPORT_TX_ALTERNATE,
                         TRANSPORT_UART, TRANSPORT_BAUD, &hUart)
    {
        pImp = this;
        initDMA();
    }
#ifdef TRANSPORT_PROTOCOL
    /**
     * return current time in milliseconds
     */
    uint32_t time_ms() override {
        return HAL_GetTick();
    }
#endif
    /**
     * transmit one byte to wire
     */
    void tx_byte(uint8_t byte) override {
        // TODO: don't do this in blocking mode
        /* while (!(this->hUart.Instance->SR & USART_SR_TXE)); */
        /* this->hUart.Instance->DR = byte; */
        HAL_UART_Transmit(&this->hUart, &byte, 1, HAL_MAX_DELAY);
    }

    /**
     * current empty space in output buffer
     */
    uint16_t tx_space() override {
        // TODO how much space is really left?
        return 128;
    }

    /**
     * connect implementation to Min
     */
    void connect(Min* min) override {
        pMin = min;
    }

    void incomingDataHandler(uint32_t nData, uint8_t *setupPointer = 0, size_t setupLen = 0) {
        // TODO: instead of handling directly in min
        // we should probably bulk-write the incoming data
        // into a min-accessible buffer, and let min handle
        // the data in min.poll()
        /* this comment is old. Considering not all devices
         * use min's transport protocol, the polling function
         * does not exist everywhere. maybe this really just
         * is the best way to do this currently? */
        static uint8_t *storage;
        static size_t s_len;
        static uint32_t old_pos = 0, pos = 0;
        // initialize handleData function
        if (setupPointer && setupLen) {
            storage = setupPointer;
            s_len = setupLen;
            return;
        }
        // calculate number of data to process
        pos = s_len - nData;
        int i = 0;
        if (pos > old_pos) {
            int32_t diff = pos - old_pos;
            while (diff--) {
                this->pMin->rx_byte(*(storage + old_pos + i++));
            }
            old_pos = (pos == s_len) ? 0 : pos;
        } else if (pos < old_pos) { // wrapped around ring buffer
            int32_t diff = s_len - old_pos;
            while (diff--) {
                this->pMin->rx_byte(*(storage + old_pos + i++));
            }
            old_pos = 0;
        }
    };

    static void dmaHTCallback(UART_HandleTypeDef *hUart) {
        pImp->incomingDataHandler(__HAL_DMA_GET_COUNTER(hUart->hdmarx));
    }

    static void dmaTCCallback(UART_HandleTypeDef *hUart) {
        pImp->incomingDataHandler(0);
    }

    static void uartCallback(UART_HandleTypeDef *hUart) {
        pImp->incomingDataHandler(__HAL_DMA_GET_COUNTER(hUart->hdmarx));
    }

    uint8_t iMessage[64] = {};

    void initDMA(void) {
        hMinDMA = {};
        hMinDMA.Instance = TRANSPORT_DMA;
#ifndef STM32F103xB
        hMinDMA.Init.Channel = TRANSPORT_DMA_CHANNEL;
#endif
        hMinDMA.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hMinDMA.Init.PeriphInc = DMA_PINC_DISABLE;
        hMinDMA.Init.MemInc = DMA_MINC_ENABLE;
        hMinDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hMinDMA.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hMinDMA.Init.Mode = DMA_CIRCULAR;
        hMinDMA.Init.Priority = DMA_PRIORITY_LOW;
#ifndef STM32F103xB
        hMinDMA.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
        while (HAL_DMA_Init(&hMinDMA) != HAL_OK);

        HAL_UART_RegisterCallback(&this->hUart, HAL_UART_RX_HALFCOMPLETE_CB_ID, this->dmaHTCallback);
        HAL_UART_RegisterCallback(&this->hUart, HAL_UART_RX_COMPLETE_CB_ID, this->dmaTCCallback);

        HAL_NVIC_SetPriority(TRANSPORT_DMA_INTERRUPT, TRANSPORT_DMA_PRIO);
        HAL_NVIC_EnableIRQ(TRANSPORT_DMA_INTERRUPT);

        __HAL_LINKDMA(&this->hUart, hdmarx, hMinDMA);
        HAL_NVIC_SetPriority(TRANSPORT_UART_INTERRUPT, TRANSPORT_UART_PRIO);
        HAL_NVIC_EnableIRQ(TRANSPORT_UART_INTERRUPT);

        __HAL_UART_CLEAR_FLAG(&this->hUart, UART_FLAG_IDLE);
        __HAL_UART_ENABLE_IT(&this->hUart, UART_IT_IDLE);
        pImp->incomingDataHandler(0, iMessage, sizeof iMessage);
        HAL_UART_Receive_DMA(&this->hUart, iMessage, sizeof iMessage);
    }

    static UART_HandleTypeDef * handle() {
        return &pImp->hUart;
    }

    private:
    static inline StmMin *pImp = nullptr;
    UART_HandleTypeDef hUart;
    DMA_HandleTypeDef hMinDMA;
    Min *pMin = nullptr;
};

extern "C" {
#ifndef TRANSPORT_UART_IRQ
#error "define TRANSPORT_UART_IRQ to use this UART implementation"
#else
void TRANSPORT_UART_IRQ(void) {
    if (__HAL_UART_GET_FLAG(StmMin::handle(), UART_FLAG_IDLE)) {
        /* __HAL_UART_CLEAR_FLAG(&hMinUart, UART_FLAG_IDLE); */
        __HAL_UART_CLEAR_IDLEFLAG(StmMin::handle());
        StmMin::uartCallback(StmMin::handle());
    }
    HAL_UART_IRQHandler(StmMin::handle());
}
#endif

#ifndef TRANSPORT_DMA_IRQ
#error "define TRANSPORT_DMA_IRQ to the rx dma to use this UART implementation"
#else
void TRANSPORT_DMA_IRQ(void)
{
  HAL_DMA_IRQHandler(StmMin::handle()->hdmarx);
}
#endif
}

#endif //STMMIN_H
