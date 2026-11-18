/** @file AS5048b.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cmath>
#include <cstdint>
#include "sys/i2c.h"

/** 14-bit rotary position sensor */
class AS5048B : I2C::Device {
public:

    /**
     * initialize the sensor. A0 & A1 change its I2C address,
     * see datasheet for details
     */
    AS5048B(Sink<I2C::Request> &bus, bool A0, bool A1)
        : I2C::Device{bus, (uint8_t)(0x40 | (A1 << 1) | A0)}
        , initialized(false)
    { }

    /**
     * start async read of angle
     */
    void measure() {
        bus.trypush({
            .dev = this,
            .data = Buffer<uint8_t>(2),
            .opts = {
                .read = true,
                .mem = true,
                },
            .mem = 0xfe,
            });
    }

    void callback(const I2C::Request &rq) override {
        if (rq.mem != 0xfe) return;
        uint16_t raw = ((uint16_t) rq.data.buf[0] << 6);
        raw += (rq.data.buf[1] & 0x3F);

        if (!initialized) {
            this->lastAngle = ((double) raw);
            initialized = true;
        } else {
            double current = ((double) raw);
            double diff = this->lastAngle - current;
            if (this->lastAngle == -1 || (this->lastAngle == 0 && (diff > 1000 || diff < -1000))) {
                diff = 0;
            } else if (diff < -10000) {
                diff += 16384;
            } else if (diff > 10000) {
                diff -= 16384;
            }

            this->angle += diff;
            this->lastAngle = current;
        }
    }

    /**
     * Returns the angle in radians
     */
    double get() {
        return this->angle / 16384 * 2 * M_PI;
    }

    /**
     * Resets the angles
     */
     void reset() {
         this->angle = 0;
         this->lastAngle = 0;
         initialized = false;
     }

private:
    ///\cond false
    bool initialized = false;

    double angle = 0;              // difference angle for rotational movement
    double lastAngle = 0;          // difference angle for rotational movement
    ///\endcond
};
