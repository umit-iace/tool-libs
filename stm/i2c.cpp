/** @file i2c.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "i2c.h"
namespace debug {
namespace i2c {
}}
using namespace I2C;
void _startMaster(HW *i2c) {
    if (i2c->handle.Init.OwnAddress1) HAL_I2C_DisableListen_IT(&i2c->handle);
    auto& rq = i2c->q.front();
    size_t sz = rq.opts.read ? rq.data.size : rq.data.len;
    i2c->active = i2c->Q;
    i2c->deadline = Deadline{uwTick + sz};
    switch(rq.opts.type) {
    case Request::Opts::MASTER_WRITE:
        HAL_I2C_Master_Transmit_IT(&i2c->handle, rq.dev->address << 1,
                                   rq.data.buf, rq.data.len);
        break;
    case Request::Opts::MASTER_READ:
        HAL_I2C_Master_Receive_IT(&i2c->handle, rq.dev->address << 1,
                                  rq.data.buf, rq.data.size);
        rq.data.len = rq.data.size;
        break;
    case Request::Opts::MEM_WRITE:
        HAL_I2C_Mem_Write_IT(&i2c->handle, rq.dev->address << 1,
                             rq.mem, I2C_MEMADD_SIZE_8BIT,
                             rq.data.buf, rq.data.len);
        break;
    case Request::Opts::MEM_READ:
        HAL_I2C_Mem_Read_IT(&i2c->handle, rq.dev->address << 1,
                            rq.mem, I2C_MEMADD_SIZE_8BIT,
                            rq.data.buf, rq.data.size);
        rq.data.len = rq.data.size;
        break;
    default:
        return;
    }
}

void _startSlave(I2C_HandleTypeDef *handle, uint8_t recv, uint16_t addr) {
    auto i2c = HW::reg.from(handle);
    assert(addr == handle->Init.OwnAddress1);
    assert(i2c->active == i2c->NONE);
    HAL_I2C_DisableListen_IT(handle);
    auto & data = recv ? i2c->in.data : i2c->out.data;
    size_t sz = recv ? data.size : data.len;
    if (!sz) return;
    i2c->active = recv ? i2c->IN : i2c->OUT;
    i2c->deadline = Deadline{uwTick + sz*2};
    if (recv) {
        HAL_I2C_Slave_Receive_IT(handle, data.buf, sz);
        data.len = sz;
    } else {
        HAL_I2C_Slave_Transmit_IT(handle, data.buf, sz);
    }
}

void poll(HW *i2c);
void _error(I2C_HandleTypeDef *handle) {
    auto i2c = HW::reg.from(handle);
    if (i2c->active == i2c->Q) {
        auto& rq = i2c->q.front();
        HAL_I2C_Master_Abort_IT(handle, rq.dev->address << 1);
        i2c->active = i2c->NONE;
        i2c->deadline = Deadline{};
        i2c->q.pop();
    } else if (i2c->active) {
        i2c->active = i2c->NONE;
        i2c->deadline = Deadline{};
    } else {
        assert(false);
    }
    __HAL_I2C_DISABLE(handle);
    __HAL_I2C_ENABLE(handle);
    if (i2c->handle.Init.OwnAddress1) HAL_I2C_EnableListen_IT(handle);
}
void poll(HW *i2c) {
    if (i2c->active && i2c->deadline(uwTick)) _error(&i2c->handle);
    if (!i2c->active && !i2c->q.empty()) return _startMaster(i2c);
}

void _complete(I2C_HandleTypeDef *handle) {
    auto i2c = HW::reg.from(handle);
    switch (i2c->active) {
    case i2c->Q:
        i2c->q.front().dev->callback(i2c->q.pop());
        break;
    case i2c->IN:
        i2c->in.dev->callback(std::move(i2c->in));
        break;
    case i2c->OUT:
        i2c->out.dev->callback(std::move(i2c->out));
        break;
    case i2c->NONE:
        assert(false);
        break;
    }
    i2c->active = i2c->NONE;
    i2c->deadline = Deadline{};
    if (i2c->handle.Init.OwnAddress1) HAL_I2C_EnableListen_IT(&i2c->handle);
    poll(i2c);
}

void HW::push(Request &&rq) {
    switch (rq.opts.type) {
        case Request::Opts::SLAVE_WRITE:
            out = std::move(rq);
            break;
        case Request::Opts::SLAVE_READ:
            in = std::move(rq);
            break;
        default:
            q.push(std::move(rq));
            break;
    }
    poll(this);
}

HW::HW(const HW::Conf &conf) {
    HW::reg.reg(this, &handle);
    handle.Instance = conf.i2c;
    handle.Init = {
#if defined(STM32F4)
        .ClockSpeed = conf.baud,
        .DutyCycle = I2C_DUTYCYCLE_2,
#elif defined(STM32F7)
        // shoot me now
        // hardcoded to 400kHz if i2cclk 216MHz
        .Timing = 0x7 << 28 | // presc
                        4 << 20 | // data setup time
                        4 << 16 | // data hold time
                        21 << 8 | // high period
                        40 << 0, // low period
#endif
        .OwnAddress1 = conf.address << 1,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .OwnAddress2 = 0,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE,
    };
    while (HAL_I2C_Init(&handle) != HAL_OK);

    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_TX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_RX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, _complete);
    if (conf.address) {
        HAL_I2C_RegisterCallback(&handle, HAL_I2C_SLAVE_TX_COMPLETE_CB_ID, _complete);
        HAL_I2C_RegisterCallback(&handle, HAL_I2C_SLAVE_RX_COMPLETE_CB_ID, _complete);
        HAL_I2C_RegisterAddrCallback(&handle, _startSlave);
        HAL_I2C_EnableListen_IT(&handle);
    }

    HAL_I2C_RegisterCallback(&handle, HAL_I2C_ERROR_CB_ID, _error);
}
void HW::irqEvHandler() {
    HAL_I2C_EV_IRQHandler(&handle);
}
void HW::irqErHandler() {
    HAL_I2C_ER_IRQHandler(&handle);
}
