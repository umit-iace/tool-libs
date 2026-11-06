/** @file QMC5883L.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include "sys/i2c.h"
#include <cmath>

/**
 * Implementation of the QMC5883L multi-chip three-axis magnetic sensor.
 *
 * see
 * https://github.com/e-Gizmo/QMC5883L-GY-271-Compass-module/blob/master/QMC5883L%20Datasheet%201.0%20.pdf
 * for details
 */
struct QMC5883L : I2C::Device {
    enum ADDR {
        DEFAULT = 0b0001101,
        ALT = 0b0001111,
    };
    static constexpr uint8_t STANDBY = 0b00000000;
    static constexpr uint8_t CONT = 0b00000001;
    static constexpr uint8_t Hz10 = 0b00000000;
    static constexpr uint8_t Hz50 = 0b00000100;
    static constexpr uint8_t Hz100 = 0b00001000;
    static constexpr uint8_t Hz200 = 0b00001100;
    static constexpr uint8_t G2 = 0b00000000;
    static constexpr uint8_t G8 = 0b00010000;
    static constexpr uint8_t S512 = 0b00000000;
    static constexpr uint8_t S256 = 0b01000000;
    static constexpr uint8_t S128 = 0b10000000;
    static constexpr uint8_t S64 = 0b11000000;

    /**
     * initialize the sensor
     */
    QMC5883L(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr} {
        // set/reset period
        writeReg(0x0B,0x01);
        // set mode
        writeReg(0x09, CONT | Hz200 | G8 | S512);
    }

    /**
     * start async read of angle
     */
    void measure() {
        bus.trypush({
            .dev = this,
            .data = Buffer<uint8_t>(9),
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = 0x00,
        });
    }

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
        if (rq.data.size != 9) return;

        // TODO Check status register
        this->Axes.x = rq.data.buf[0] | (rq.data.buf[1] << 8);
        this->Axes.y = rq.data.buf[2] | (rq.data.buf[3] << 8);
        this->Axes.z = rq.data.buf[4] | (rq.data.buf[5] << 8);
        this->Temperature = ((double) (rq.data.buf[7] | (rq.data.buf[8] << 8)));

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
    double Temperature = 0;        // current temperature
    struct {
        int16_t x, y, z;
    } Axes;

    double heading = 0;
    double declinationAngle = 0.0303; // http://www.magnetic-declination.com/
};
