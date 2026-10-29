/** @file spi.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "sys/spi.h"

using namespace SPI;
namespace SPI {
/** generous deadline for transferring datasize bytes */
Deadline until(SPI_HandleTypeDef *handle, size_t datasize, uint32_t now) {
    // SPI peripherals sit on either APB, assume the slower one
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    if (HAL_RCC_GetPCLK2Freq() < pclk) pclk = HAL_RCC_GetPCLK2Freq();
    uint32_t div = 2U << ((handle->Init.BaudRatePrescaler & SPI_CR1_BR) >> SPI_CR1_BR_Pos);
    uint32_t bits_per_ms = pclk / div / 1000;
    if (!bits_per_ms) bits_per_ms = 1;
    // + 2ms margin, so short transfers don't time out on tick granularity
    return Deadline::after(now, (uint32_t)(datasize * 8 / bits_per_ms) + 2);
}
}

void switchmode(HW *spi, Device::Conf newconf) {
    while (__HAL_SPI_GET_FLAG(&spi->handle, SPI_FLAG_BSY)) ;
    __HAL_SPI_DISABLE(&spi->handle);
    auto &init = spi->handle.Init;
    init.FirstBit = newconf.fsb == FirstBit::MSB ? SPI_FIRSTBIT_MSB : SPI_FIRSTBIT_LSB;
    switch(newconf.mode) {
    case Mode::M0:
        init.CLKPolarity = SPI_POLARITY_LOW;
        init.CLKPhase = SPI_PHASE_1EDGE;
        break;
    case Mode::M1:
        init.CLKPolarity = SPI_POLARITY_LOW;
        init.CLKPhase = SPI_PHASE_2EDGE;
        break;
    case Mode::M2:
        init.CLKPolarity = SPI_POLARITY_HIGH;
        init.CLKPhase = SPI_PHASE_1EDGE;
        break;
    case Mode::M3:
        init.CLKPolarity = SPI_POLARITY_HIGH;
        init.CLKPhase = SPI_PHASE_2EDGE;
        break;
    }
    while (HAL_SPI_Init(&spi->handle)) ;

    spi->active.conf = newconf;
}

void _startMaster(HW *spi) {
    auto& rq = spi->q.front();
    size_t sz = rq.dir & Request::MOSI ? rq.data.len : rq.data.size;

    if (spi->active.conf != rq.dev->conf) switchmode(spi, rq.dev->conf);
    // activate the chip
    rq.dev->select(true);
    spi->deadline = until(&spi->handle, sz, uwTick);

    // transfer the data
    switch (rq.dir) {
    case Request::MOSI:
        HAL_SPI_Transmit_IT(&spi->handle, rq.data, sz);
        break;
    case Request::MISO:
        HAL_SPI_Receive_IT(&spi->handle, rq.data, sz);
        rq.data.len = rq.data.size;
        break;
    case Request::BOTH:
        HAL_SPI_TransmitReceive_IT(&spi->handle, rq.data, rq.data, sz);
        break;
    }
    spi->active.xfer = true;
}


void _error(SPI_HandleTypeDef *handle);
void poll(HW *spi) {
    if (spi->active.xfer && spi->deadline(uwTick)) _error(&spi->handle);
    if (!spi->active.xfer && !spi->q.empty()) return _startMaster(spi);
}

/** stop the transfer on register level
 *
 * HAL_SPI_Abort(_IT) busy wait for the SPI interrupt to acknowledge, which
 * never comes inside the CriticalSection of push() (~30ms at 216MHz)
 */
void _abort(SPI_HandleTypeDef *handle) {
    CLEAR_BIT(handle->Instance->CR2, SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE);
    __HAL_SPI_DISABLE(handle);
    while (__HAL_SPI_GET_FLAG(handle, SPI_FLAG_RXNE)) (void)handle->Instance->DR;
    __HAL_SPI_CLEAR_OVRFLAG(handle);
    handle->TxXferCount = 0;
    handle->RxXferCount = 0;
    handle->State = HAL_SPI_STATE_READY;
    __HAL_UNLOCK(handle);
}

void _error(SPI_HandleTypeDef *handle) {
    auto spi = HW::reg.from(handle);
    if (!spi) return;
    if (spi->active.xfer) {
        auto &rq = spi->q.front();
        rq.dev->select(false);
        _abort(handle);
        // give up on this request, a broken device would block the bus
        // forever otherwise. drop instead of pop: don't free memory in here
        spi->q.drop();
        spi->deadline = {};
        spi->active.xfer = false;
    }
    __HAL_SPI_DISABLE(handle);
    __HAL_SPI_ENABLE(handle);
    // called from the HAL error interrupt as well, nobody else restarts the queue
    if (!spi->q.empty()) _startMaster(spi);
}

void _complete(SPI_HandleTypeDef *handle) {
    auto spi = HW::reg.from(handle);
    if (!spi) return;
    auto& rq = spi->q.front();
    rq.dev->select(false);
    rq.dev->callback(rq);
    spi->q.drop();
    spi->deadline = {};
    spi->active.xfer = false;
    poll(spi);
}

bool HW::full() {
    return q.full();
}

void HW::push(Request &&rq) {
    // state is shared with the interrupt handlers
    CriticalSection cs;
    q.push(std::move(rq));
    poll(this);
}

HW::HW(const HW::Conf &conf) {
    halInit();
    HW::reg.reg(this, &handle);
    handle = {
        .Instance = conf.spi,
        .Init = {
            .Mode = SPI_MODE_MASTER,
            .NSS = SPI_NSS_SOFT,
            .BaudRatePrescaler = conf.prescaler,
        },
    };
    while(HAL_SPI_Init(&handle) != HAL_OK);

    HAL_SPI_RegisterCallback(&handle, HAL_SPI_TX_COMPLETE_CB_ID, _complete);
    HAL_SPI_RegisterCallback(&handle, HAL_SPI_RX_COMPLETE_CB_ID, _complete);
    HAL_SPI_RegisterCallback(&handle, HAL_SPI_TX_RX_COMPLETE_CB_ID, _complete);
    HAL_SPI_RegisterCallback(&handle, HAL_SPI_ERROR_CB_ID, _error);

}

void HW::irqHandler() {
    HAL_SPI_IRQHandler(&handle);
}
