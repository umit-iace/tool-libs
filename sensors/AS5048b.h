/** @file AS5048B_H.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cmath>
#include <cstdint>
#include "stm/i2c.h"

class AS5048B : I2C::Device {
public:

    /**
     * initialize the sensor
     */
    AS5048B(Sink<I2C::Request> *bus, bool A0, bool A1)
        : I2C::Device{*bus, (uint8_t)(0x40 | (A1 << 1) | A0)}
        , bInit(false)
    { }

    /**
     * start async read of angle
     */
    void measure() {
        bus.push({
            .dev = this,
            .data = Buffer<uint8_t>{2},
            .opts = {
                .read = true,
                .mem = true,
                },
            .mem = 0xfe,
            });
    }

    void callback(const I2C::Request &rq) override {
        if (rq.mem != 0xfe) return;
        uint16_t iRawAngle = ((uint16_t) rq.data.buf[0] << 6);
        iRawAngle += (rq.data.buf[1] & 0x3F);

        if (!bInit) {
            this->dLastAngle = ((double) iRawAngle);
            bInit = true;
        } else {
            double dCurAngle = ((double) iRawAngle);
            double dDiffAngle = this->dLastAngle - dCurAngle;
            if (this->dLastAngle == -1 || (this->dLastAngle == 0 && (dDiffAngle > 1000 || dDiffAngle < -1000))) {
                dDiffAngle = 0;
            } else if (dDiffAngle < -10000) {
                dDiffAngle += 16384;
            } else if (dDiffAngle > 10000) {
                dDiffAngle -= 16384;
            }

            this->dAngle += dDiffAngle;
            this->dLastAngle = dCurAngle;
        }
    }

    /**
     * Returns the angle in radians
     */
    double get() {
        return this->dAngle / 16384 * 2 * M_PI;
    }

private:
    ///\cond false
    bool bInit = false;

    double dAngle = 0;              // difference angle for rotational movement
    double dLastAngle = 0;          // difference angle for rotational movement
    ///\endcond
};
