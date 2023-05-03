/** @file QMC5883L.h
 *
 * Copyright (c) 2023 IACE
 */
#ifndef QMC5883L_H
#define QMC5883L_H

#include "stm/hal.h"
#include "stm/i2c.h"
#include <cmath>

/**
 * Implementation of the QMC5883L multi-chip three-axis magnetic sensor.
 *
 * see
 * for details
 */
struct QMC5883L : I2C::Device {
    enum ADDR {
        DEFAULT = 0x1e,
    };
    enum MODE {
        STANDBY = 0b00000000,
        CONT = 0b00000001,
    };
    enum ODR {
        Hz10 = 0b00000000,
        Hz50 = 0b00000100,
        Hz100 = 0b00001000,
        Hz200 = 0b00001100,
    };
    enum RANGE {
        G2 = 0b00000000,
        G8 = 0b00010000,
    };
    enum OSR {
        S512 = 0b00000000,
        S256 = 0b01000000,
        S128 = 0b10000000,
        S64 = 0b11000000,
    };

    /**
     * initialize the sensor
     */
    QMC5883L(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr} {
        // set/reset period
        writeReg(2,0);
        // set mode
//        writeReg(0x09, CONT | Hz200 | G8 | S512);
    }

    /**
     * start async read of angle
     */
    void measure() {
        bus.push({
            .dev = this,
            .data = Buffer<uint8_t>{6},
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = 0x3,
        });
    }

    void writeReg(uint8_t reg, uint8_t val) {
        bus.push({
            .dev = this,
            .data = Buffer<uint8_t>{1}.append(val),
            .opts = {
                .mem = true,
            },
            .mem = reg,
        });
    }

    void callback(const I2C::Request rq) override {
        if (rq.data.size != 6) return;

        // TODO Check status register
        this->Axes.x = rq.data.buf[1] | (rq.data.buf[0] << 8);
        this->Axes.y = rq.data.buf[3] | (rq.data.buf[2] << 8);
        this->Axes.z = rq.data.buf[5] | (rq.data.buf[4] << 8);

        this->heading = atan2(this->Axes.y, this->Axes.x);

        this->heading += this->declinationAngle;
        if(this->heading < 0)
            this->heading += 2 * M_PI;
        if(this->heading > 2 * M_PI)
            this->heading -= 2 * M_PI;
    }

    double getHeadingDegree() {
        return heading * 180 / M_PI;
    }

private:
    struct {
        uint16_t x, y, z;
    } Axes;

    double heading = 0;
    double declinationAngle = 0.0303; // http://www.magnetic-declination.com/
};

#endif //QMC5883L_H