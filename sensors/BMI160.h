/** @file BMI160.h
 *
 * Copyright (c) 2022 IACE
 */
#ifndef BMI160_H
#define BMI160_H

#include <cstdint>
#include "stm/i2c.h"

struct BMI160 : I2CDevice {
    enum ADDR {
        DEFAULT = 0b1101000,
        ALT = 0b1101001,
    };
    struct Axes {
        double x, y, z;
    };

    BMI160(enum ADDR addr=DEFAULT) : I2CDevice(addr) {
        writeReg(0x7e, 0x11);   // enable accelerometer
        writeReg(0x7e, 0x15);   // enable gyroscope
        uint8_t reg = 0x0c;     // set read pointer to gyro data
        HardwareI2C::master()->request(new I2CRequest(
                this,
                0,
                &reg,
                1,
                I2CRequest::I2C_DIRECT_WRITE,
                nullptr)
        );
    }

    /**
     * start async read-out of sensor data
     */
    void measure() {
        HardwareI2C::master()->request(new I2CRequest(
                this,
                0,
                (uint8_t *) this->raw,
                12,
                I2CRequest::I2C_DIRECT_READ,
                (void *) true)
        );
    }

    int16_t raw[6]{};
    Axes acc{}, gyro{};
    // TODO: make ranges configurable
    double factor_a{9.81 / 16384}, factor_g{3.14 / 180 / 16.4};

    void writeReg(uint8_t reg, uint8_t val) {
        HardwareI2C::master()->request(new I2CRequest(
                this,
                reg,
                &val,
                1,
                I2CRequest::I2C_MEM_WRITE,
                nullptr)
        );
    }

    void callback(void *cbData) override {
        Axes g{
                .x = raw[0] * factor_g,
                .y = raw[1] * factor_g,
                .z = raw[2] * factor_g,
        };
        Axes a{
                .x = raw[3] * factor_a,
                .y = raw[4] * factor_a,
                .z = raw[5] * factor_a,
        };
        this->gyro = g;
        this->acc = a;
    }
};

#endif //BMI160_H
