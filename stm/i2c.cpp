/** @file i2c.cpp
 *
 * Copyright (c) 2023 IACE
 */
#include "i2c.h"
namespace debug {
namespace i2c {
}}
using namespace I2C;
void start(HW *i2c) {
    if (i2c->q.empty()) return;
    auto& rq = i2c->q.front();
    /* assert(rq.mem && !rq.opts.slave);// mem op not yet implemented */
    i2c->active=true;
    i2c->deadline = Deadline{uwTick+rq.data.size*2};
    switch(rq.opts.type) {
    case Request::Opts::MASTER_WRITE:
        HAL_I2C_Master_Transmit_IT(&i2c->handle, rq.dev->address << 1,
                                   rq.data.buf, rq.data.len);
        break;
    case Request::Opts::MASTER_READ:
        HAL_I2C_Master_Receive_IT(&i2c->handle, rq.dev->address << 1,
                                  rq.data.buf, rq.data.size);
        break;
    case Request::Opts::SLAVE_WRITE:
        HAL_I2C_Slave_Transmit_IT(&i2c->handle, rq.data.buf, rq.data.len);
        break;
    case Request::Opts::SLAVE_READ:
        HAL_I2C_Slave_Receive_IT(&i2c->handle, rq.data.buf, rq.data.size);
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
        break;
    }
    /* if (rq.opts.mem) { */
    /*     if(rq.opts.read) { */
    /*         HAL_I2C_Mem_Read_IT(&i2c->handle, rq.dev->address << 1, */
    /*                             rq.mem, I2C_MEMADD_SIZE_8BIT, */
    /*                             rq.data.buf, rq.data.size); */
    /*     } else { */
    /*         HAL_I2C_Mem_Write_IT(&i2c->handle, rq.dev->address << 1, */
    /*                              rq.mem, I2C_MEMADD_SIZE_8BIT, */
    /*                              rq.data.buf, rq.data.len); */
    /*     } */
    /* } else { */
    /*     if (rq.opts.slave) { */
    /*         if (rq.opts.read){ */
    /*         HAL_I2C_Slave_Receive_IT(&i2c->handle, rq.data.buf, rq.data.size); */
    /*         } else { */
    /*         HAL_I2C_Slave_Transmit_IT(&i2c->handle, rq.data.buf, rq.data.len); */
    /*         } */
    /*     } else { */
    /*         if (rq.opts.read){ */
    /*         HAL_I2C_Master_Receive_IT(&i2c->handle, rq.dev->address << 1, */
    /*                                   rq.data.buf, rq.data.size); */
    /*         } else { */
    /*         HAL_I2C_Master_Transmit_IT(&i2c->handle, rq.dev->address << 1, */
    /*                                    rq.data.buf, rq.data.len); */
    /*         } */
    /*     } */
    /* } */
}
void _error(I2C_HandleTypeDef *handle) {
    auto i2c = HW::reg.from(handle);
    if (i2c->active) {
        auto& rq = i2c->q.front();
        if (HAL_I2C_Master_Abort_IT(&i2c->handle, rq.dev->address << 1) != HAL_OK) {
            i2c->active = false;
        }
        i2c->q.pop();
        i2c->deadline = Deadline{};
    }
}
void _aborted(I2C_HandleTypeDef *handle) {
    auto i2c = HW::reg.from(handle);
    i2c->active = false;
}

void poll(HW *i2c) {
    if (i2c->active && i2c->deadline(uwTick)) return _error(&i2c->handle);
    if (!i2c->active) return start(i2c);
}

void _complete(I2C_HandleTypeDef *handle) {
    auto i2c = HW::reg.from(handle);
    auto rq = i2c->q.pop();
    rq.dev->callback(rq);
    i2c->active = false;
    i2c->deadline = Deadline{};
    poll(i2c);
}
void HW::push(Request &&rq) {
    q.push(std::move(rq));
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
        .OwnAddress1 = conf.address,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .OwnAddress2 = 0,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE,
    };
    while (HAL_I2C_Init(&handle) != HAL_OK);
    timeout = conf.timeout;

    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_TX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_RX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_SLAVE_TX_COMPLETE_CB_ID, _complete);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_SLAVE_RX_COMPLETE_CB_ID, _complete);

    HAL_I2C_RegisterCallback(&handle, HAL_I2C_ERROR_CB_ID, _error);
    HAL_I2C_RegisterCallback(&handle, HAL_I2C_ABORT_CB_ID, _aborted);
}
void HW::irqEvHandler() {
    HAL_I2C_EV_IRQHandler(&handle);
}
void HW::irqErHandler() {
    HAL_I2C_ER_IRQHandler(&handle);
}
