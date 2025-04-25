/** @file BNO085.h
 *
 * Copyright (c) 2025 IACE
 */
#pragma once

#include "sys/i2c.h"
#include <cmath>
#include <core/logger.h>
extern Logger lg;

/**
 * Implementation of the BNO085 9-axis motion processor
 *
 * see
 * https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf
 * https://cdn-learn.adafruit.com/downloads/pdf/adafruit-9-dof-orientation-imu-fusion-breakout-bno085.pdf
 * for details
 *
 */

struct BNO085 : I2C::Device {
    enum ADDR {
        DEFAULT = 0b1001010,
        ALT = 0b1001011,
    };
    struct Axis {
        double x,y,z;
    };
    struct Raw {
        int16_t x,y,z;
    } accel {}, gyro {}, mag {};

    M92(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr}, magdev{bus} {
    }
    void init() {

    }

    void measure() {
        bus.trypush({
            .dev = this,
            .data = 20,
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = ACCEL_X,
        });
    }

    void callback(const I2C::Request &rq) override {
        char data[128];
    }

    Axis get_accel() {
        return {
            .x = accel.x,
            .y = accel.y,
            .z = accel.z,
        };
    }
    Axis get_gyro() {
        return {
            .x = gyro.x,
            .y = gyro.y,
            .z = gyro.z,
        };
    }
    Axis get_mag() {
        return {
            .x = mag.x,
            .y = mag.y,
            .z = mag.z,
        };
    }

private:
    void read(uint8_t addr) {
        bus.trypush({
            .dev = this,
            .data = 1,
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = addr,
        });
    }
    void write(uint8_t addr, uint8_t val) {
        bus.trypush({
            .dev = this,
            .data = {val},
            .opts = {
                .mem = true,
            },
            .mem = addr,
        });
    }
};
