/** @file i2c.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <utils/queue.h>
#include <utils/deadline.h>

#include "gpio.h"
#include "registry.h"
namespace I2C {
class Request; //< forward declaration
/**
 * generic I2C device wrapper.
 *
 * Implement I2C devices with this as a base class to make use of the
 * asynchronous HardwareI2C bus.
 *
 * @note currently only implements 7bit addressing
 */
struct Device {
    Sink<Request> &bus;
    /// unshifted 7bit I2C device address
    const uint8_t address;
    Device(Sink<Request> &bus, uint8_t address) : bus(bus), address(address) {}
    /**
     * callback. is called as soon as transmission completed successfully
     */
    virtual void callback(const Request req)=0;
};

/**
 * struct defining an I2C request.
 * use this when transmitting data over the I2C bus
 */
struct Request {
    Device *dev; ///< pointer to the calling I2C::Device instance
    Buffer<uint8_t> data{0}; ///< transfer data
    union Opts {
        struct {
        uint8_t read:1;
        uint8_t slave:1;
        uint8_t mem:1;
        };
        enum {
            MASTER_WRITE,
            MASTER_READ,
            SLAVE_WRITE,
            SLAVE_READ,
            MEM_WRITE,
            MEM_READ,
        } type;
    } opts;
    uint8_t mem; ///< memory address for memory transmissions
};

struct HW : public Sink<Request> {
    inline static Registry<HW, I2C_HandleTypeDef, 4> reg{};

    Queue<Request> q; //< master
    Request in, out; //< slave
    Deadline deadline{};
    enum {NONE, IN, OUT, Q} active{};

    struct Conf {
        I2C_TypeDef *i2c; ///< I2C peripheral
        AFIO sda, scl; ///< initialized (Open-Drain) pins
        uint32_t baud; ///< baud-rate
        uint32_t address; ///< unshifted 7-bit address to respond to as slave
    };
    HW(const Conf &conf);
    void push(Request &&rq) override;
    using Sink<Request>::push;
    void irqEvHandler();
    void irqErHandler();
    I2C_HandleTypeDef handle{};
};
}
