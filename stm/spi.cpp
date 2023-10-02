/** @file spi.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "spi.h"

using namespace SPI;
static Deadline until(size_t datasize) {
    return {};
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
    spi->deadline = until(sz);

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

void _error(SPI_HandleTypeDef *handle) {
    auto spi = HW::reg.from(handle);
    auto &rq= spi->q.front();
    if (spi->active.xfer) {
        rq.dev->select(false);
        HAL_SPI_Abort_IT(handle);
        spi->deadline = {};
        spi->active.xfer = false;
    }
    __HAL_SPI_DISABLE(handle);
    __HAL_SPI_ENABLE(handle);
}

void _complete(SPI_HandleTypeDef *handle) {
    auto spi = HW::reg.from(handle);
    auto& rq = spi->q.front();
    rq.dev->select(false);
    rq.dev->callback(rq);
    spi->q.pop();
    spi->deadline = {};
    spi->active.xfer = false;
    poll(spi);
}

void HW::push(Request &&rq) {
    q.push(std::move(rq));
    poll(this);
}

HW::HW(const HW::Conf &conf) {
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
