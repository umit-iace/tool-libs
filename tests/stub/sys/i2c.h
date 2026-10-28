/** @file i2c.h
 *
 * minimal stand-in for stm/sys/i2c.h, so I2C device drivers can be
 * tested on the host. keep in sync with the real Device & Request.
 */
#pragma once
#include <utils/queue.h>

namespace I2C {
class Request;
struct Device {
    Sink<Request> &bus;
    const uint8_t address;
    Device(Sink<Request> &bus, uint8_t address) : bus(bus), address(address) {}
    virtual void callback(const Request &req)=0;
};

struct Request {
    Device *dev;
    Buffer<uint8_t> data;
    union Type {
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
    uint8_t mem;
};
}
