/** @file BMI160.h
 *
 * Copyright (c) 2022 IACE
 */
#pragma once

#include <cstdint>
#include "sys/i2c.h"

/** Implementation of the BMI160 6axis Inertial Measurement Unit */
struct BMI160 : I2C::Device {
    enum ADDR {
        DEFAULT = 0b1101000,
        ALT = 0b1101001,
    };
    struct Axes {
        double x, y, z;
    };

    BMI160(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
            : I2C::Device(bus, addr) {
        writeReg(0x7e, 0x11);   // enable accelerometer
        writeReg(0x7e, 0x15);   // enable gyroscope
        bus.trypush({
            .dev = this,
            .data = {0xc}, // set read pointer to gyro data
            });
    }

    /**
     * start async read-out of sensor data
     */
    void measure() {
        bus.trypush({
            .dev = this,
            .data = 12,
            .opts = {
                .read = true,
                },
            });
    }

    Axes acc{}, gyro{};
    // TODO: make ranges configurable
    double factor_a{9.81 / 16384}, factor_g{3.14 / 180 / 16.4};

    void writeReg(uint8_t reg, uint8_t val) {
        bus.trypush({
            .dev = this,
            .data = {val},
            .opts = {
                .mem = true,
                },
            .mem = reg,
        });
    }

    void callback(const I2C::Request &rq) override {
        if (rq.data.size != 12) return;
        auto view = (int16_t*)rq.data.buf;
        Axes g{
                .x = view[0] * factor_g,
                .y = view[1] * factor_g,
                .z = view[2] * factor_g,
        };
        Axes a{
                .x = view[3] * factor_a,
                .y = view[4] * factor_a,
                .z = view[5] * factor_a,
        };
        this->gyro = g;
        this->acc = a;
    }
};
