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
 * asynchronous I2C::HW bus.
 *
 * @note currently only implements 7bit addressing
 */
struct Device {
    Sink<Request> &bus;
    /// unshifted 7bit I2C device address
    const uint8_t address;
    /** construct device on bus with given address */
    Device(Sink<Request> &bus, uint8_t address) : bus(bus), address(address) {}
    /**
     * callback. is called as soon as transmission completed successfully
     */
    virtual void callback(const Request req)=0;
};

/**
 * struct defining an I2C request.
 * use this for transmitting data over an I2C::HW bus
 */
struct Request {
    Device *dev; ///< pointer to the calling I2C::Device instance
    Buffer<uint8_t> data; ///< transfer data
    union Type {
        struct {
        uint8_t read:1; ///< read transmission
        uint8_t slave:1; ///< as slave
        uint8_t mem:1; ///< memory transmission
        };
        enum {
            MASTER_WRITE,
            MASTER_READ,
            SLAVE_WRITE,
            SLAVE_READ,
            MEM_WRITE,
            MEM_READ,
        } type;
    } opts; /**< transfer options. bitfield with fields
              *  name | comment
              * ------|------------------
              *  read | read transmission
              *  slave| as slave
              *  mem  | memory transmission
              */
    uint8_t mem; ///< memory address for memory transmissions
};

/** I2C Peripheral Driver */
struct HW : public Sink<Request> {
    inline static Registry<HW, I2C_HandleTypeDef, 4> reg{};

    Queue<Request> q; //< master
    Request in, out; //< slave
    Deadline deadline{};
    enum {NONE, IN, OUT, Q} active{};

    /** bus configuration */
    struct Conf {
        I2C_TypeDef *i2c; ///< I2C peripheral
        AFIO sda, scl; ///< initialized (Open-Drain) pins
        uint32_t baud; ///< baud-rate
        uint32_t address; ///< unshifted 7-bit address to respond to as slave
    };
    /** init peripheral with given Conf */
    HW(const Conf &conf);
    void push(Request &&rq) override;
    using Sink<Request>::push;
    void irqEvHandler();
    void irqErHandler();
    void poll(uint32_t now, uint32_t dt);
    I2C_HandleTypeDef handle{};
};
}
