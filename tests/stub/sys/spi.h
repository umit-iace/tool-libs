/** @file spi.h
 *
 * minimal stand-in for stm/sys/spi.h, so SPI device drivers can be
 * tested on the host. keep in sync with the real Device & Request.
 */
#pragma once
#include <utils/queue.h>

/** chipselect pin stand-in */
struct DIO {
    bool level = true;
    void set(bool high) { level = high; }
};

namespace SPI {
class Request;
enum class Mode { M0, M1, M2, M3 };
enum class FirstBit { MSB, LSB };
struct Device {
    Sink<Request> &bus;
    DIO cs;
    struct Conf {
        Mode mode;
        FirstBit fsb;
    } const conf;
    Device(Sink<Request> &bus, DIO cs, Conf c) : bus(bus), cs(cs), conf(c) {
        select(false);
    }
    void select(bool sel) {
        cs.set(!sel);
    }
    virtual void callback(const Request &req)=0;
    virtual ~Device() = default;
};

struct Request {
    Device *dev;
    Buffer<uint8_t> data;
    enum Direction {
        MOSI = 1,
        MISO = 2,
        BOTH = 3,
    } dir;
};
}
