/** @file spi.h
 *
 * Copyright (c) 2023 IACE
 */

#pragma once
#include <utils/queue.h>
#include <utils/deadline.h>

#include "gpio.h"
#include "registry.h"
namespace SPI {
class Request; //< forward declaration
enum class Mode { M0, M1, M2, M3 }; //< SPI Modes 0 - 3
enum class FirstBit { MSB, LSB }; //< transmission first bit significance
/**
 * generic SPI device wrapper.
 *
 * Implement SPI devices with this as a base class to make use of the
 * asynchronous SPI::HW bus.
 */
struct Device {
    Sink<Request> &bus;
    /// GPIO chipselect line
    DIO cs;
    /// SPI Configuration
    struct Conf {
        Mode mode; ///< Mode configuration of SPI device
        FirstBit fsb; ///< first bit significance
        bool operator!=(const Conf &o) {
            return mode != o.mode || fsb != o.fsb;
        }
    } const conf; ///< necessary SPI configuration
    Device(Sink<Request> &bus, DIO cs, Conf c) : bus(bus), cs(cs), conf(c) {
        select(false);
    }

    /**
     * chipselect activation
     * @param sel true/false
     */
    void select(bool sel) {
        cs.set(!sel);
    }

    /** is called as soon as requested data arrived over wire */
    virtual void callback(const Request req)=0;
};

/**
 * struct defining an SPI request.
 * use this for transmitting data over an SPI::HW bus
 */
struct Request {
    Device *dev;     ///< pointer to the calling SPI::Device instance
    Buffer<uint8_t> data; ///< transfer data
    enum Direction {
        MOSI = 1, ///< Master out, Slave in
        MISO = 2, ///< Master in, Slave out
        BOTH = 3, ///< Both in, Both out
    } dir; ///< direction the data should travel
};

/** SPI Peripheral Driver
 *
 * @note only Master communication supported for now
 */
struct HW : public Sink<Request> {
    inline static Registry<HW, SPI_HandleTypeDef, 4> reg{};

    Queue<Request> q;
    Deadline deadline{};
    struct {
        bool xfer;
        Device::Conf conf;
    } active{};

    /** peripheral configuration */
    struct Conf {
        SPI_TypeDef *spi; ///< SPI peripheral
        AFIO miso, mosi, sck; ///< initialized pins
        uint32_t prescaler; ///< must be of value SPI_BaudRate_Prescaler
                            /// from the STM HAL
    };
    /** init peripheral with given Conf */
    HW(const Conf &conf);
    bool full() override;
    void push(Request &&rq) override;
    using Sink<Request>::push;
    void irqHandler();
    SPI_HandleTypeDef handle{};
};
}
