/** @file MAX31855.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once

#include <cmath>
#include <cstdint>

#include "sys/spi.h"

/** Implementation of MAX31855 thermocouple temperature measurement */
struct MAX31855 : SPI::Device {
    /**
     * Constructor
     * @param bus SPI bus
     * @param cs chip select pin
     */
    MAX31855(Sink<SPI::Request> &bus, DIO cs)
        : SPI::Device(bus, cs, {SPI::Mode::M0, SPI::FirstBit::MSB}) { }

    /**
     * measured sensor temperature
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double temp() {
        if (data.FAULT) return NAN;
        return data.TEMP * 0.25;
    }

    /**
     * measured internal chip temperature
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double internal() {
        if (data.FAULT) return NAN;
        return data.INTERNAL * 0.0625;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        bus.push({
            .dev = this,
            .data = 4,
            .dir = SPI::Request::MISO,
            });
    }

    void callback(const SPI::Request rq) override {
        int32_t raw = rq.data.buf[0]<<24 | rq.data.buf[1]<<16 | rq.data.buf[2]<<8 | rq.data.buf[3];
        data = *(sensor *)&raw;
    }

    struct sensor {
        uint8_t OC:1;                               ///< \b error: open circuit
        uint8_t SCG:1;                              ///< \b error: short to GND
        uint8_t SCV:1;                              ///< \b error: short to VCC
        uint8_t :1;
        int16_t INTERNAL:12;                       ///< internal measured temperature in 0.0625°C steps
        uint8_t FAULT:1;                            ///< \b error: fault in thermocouple reading
        uint8_t :1;
        int16_t TEMP:14;                           ///< thermocouple measured temperature in 0.25°C steps
    } data = {};                              ///< sensor data struct. lsb to msb order.
};
