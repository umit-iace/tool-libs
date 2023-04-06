/** @file AS5048B_H.h
 *
 * Copyright (c) 2023 IACE
 */
#ifndef AS5048B_H
#define AS5048B_H

#include <cstdint>
#include "stm/hal.h"
#include "stm/i2c.h"

#define M_PI 3.14159265358979323846

class AS5048B : I2CDevice {
public:

    /**
     * initialize the sensor
     */
    AS5048B(RequestQueue<I2CRequest> *bus,
            bool A0, bool A1) : I2CDevice((0x40 << 0) | ((A0 ? 1 : 0) << 0) | ((A1 ? 1 : 0) << 1)),
                                bInit(false), bus(bus) {
    }

    /**
     * start async read of angle
     */
    void measure() {
        bus->request(new I2CRequest(
                this,
                0xfe,
                (uint8_t *) this->raw,
                2,
                I2CRequest::I2C_MEM_READ,
                (void *) true)
        );
    }

    void writeReg(uint8_t reg, uint8_t val) {
        bus->request(new I2CRequest(
                this,
                reg,
                &val,
                1,
                I2CRequest::I2C_MEM_WRITE,
                nullptr)
        );
    }

    void callback(void *cbData) override {
        uint16_t iRawAngle = ((uint16_t) this->raw[0] << 6);
        iRawAngle += (this->raw[1] & 0x3F);

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
     * Returns the angle in radiant
     */
    double get() {
        return this->dAngle / 16384 * 2 * M_PI;
    }

private:
    ///\cond false
    uint8_t raw[2] = {};
    bool bInit = false;

    double dAngle = 0;              // difference angle for rotational movement
    double dLastAngle = 0;          // difference angle for rotational movement
    RequestQueue<I2CRequest> *bus;

    ///\endcond
};

#endif //AS5048B_H