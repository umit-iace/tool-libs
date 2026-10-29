/** @file i2c.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <utils/queue.h>
#include <utils/deadline.h>
#include <core/critical.h>

#include "gpio.h"
#include "registry.h"
#include "deferred.h"
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
    /** runs in the main loop (Kernel scheduler), not in the interrupt */
    virtual void callback(const Request &req)=0;
    virtual ~Device() = default;
};

/**
 * struct defining an I2C request.
 * use this for transmitting data over an I2C::HW bus
 */
struct Request {
    Device *dev; ///< pointer to the calling I2C::Device instance
    Buffer<uint8_t> data; ///< transfer data
    /** transfer options. bitfield with fields
     *  name | comment
     * ------|------------------
     *  read | read transmission
     *  slave| as slave
     *  mem  | memory transmission
     */
    struct Type {
        uint8_t read:1; ///< read transmission
        uint8_t slave:1; ///< as slave
        uint8_t mem:1; ///< memory transmission
        /** the combinations of the bits above */
        enum Kind {
            MASTER_WRITE,
            MASTER_READ,
            SLAVE_WRITE,
            SLAVE_READ,
            MEM_WRITE,
            MEM_READ,
        };
        /** which kind of transfer the bits describe */
        Kind type() const { return Kind(read | slave << 1 | mem << 2); }
        /** set the bits for given kind of transfer */
        void set(Kind k) {
            read = k & 1;
            slave = k >> 1 & 1;
            mem = k >> 2 & 1;
        }
    } opts;
    uint8_t mem; ///< memory address for memory transmissions
};

/** I2C Peripheral Driver */
struct HW : public Sink<Request> {
    inline static Registry<HW, I2C_HandleTypeDef, 4> reg{};

    Queue<Request> q; //< master
    /** completed requests, their callbacks run in the main loop */
    Deferred<Request> completed;
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
    /** no need to call this before pushing SLAVE requests */
    bool full() override;
    void push(Request &&rq) override;
    using Sink<Request>::push;
    void irqEvHandler();
    void irqErHandler();
    I2C_HandleTypeDef handle{};
};
}
