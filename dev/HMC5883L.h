/** @file HMC5883L.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include "sys/i2c.h"
#include <cmath>

/**
 * Implementation of the HMC5883L multi-chip three-axis magnetic sensor.
 *
 * see
 * for details
 */
struct HMC5883L : I2C::Device {
    enum ADDR {
        DEFAULT = 0x1e,
    };
    static constexpr uint8_t CONT = 0b00000000;
    static constexpr uint8_t SINGLE = 0b00000001;
    static constexpr uint8_t IDLE = 0b00000011;
    static constexpr uint8_t S1 = 0b00000000;
    static constexpr uint8_t S2 = 0b00100000;
    static constexpr uint8_t S4 = 0b01000000;
    static constexpr uint8_t S8 = 0b01100000;
    static constexpr uint8_t Hz0_75 = 0b0000000;
    static constexpr uint8_t Hz1_5 = 0b00000100;
    static constexpr uint8_t Hz3 = 0b00001000;
    static constexpr uint8_t Hz7_5 = 0b00001100;
    static constexpr uint8_t Hz15 = 0b00010000;
    static constexpr uint8_t Hz30 = 0b00010100;
    static constexpr uint8_t Hz75 = 0b00011000;
    static constexpr uint8_t G0_88 = 0b00000000;
    static constexpr uint8_t G1_3 = 0b00100000;
    static constexpr uint8_t G1_9 = 0b01000000;
    static constexpr uint8_t G2_5 = 0b01100000;
    static constexpr uint8_t G4_0 = 0b10000000;
    static constexpr uint8_t G4_7 = 0b10100000;
    static constexpr uint8_t G5_6 = 0b11000000;
    static constexpr uint8_t G8_1 = 0b11100000;
    static constexpr uint8_t NORMAL = 0b00000000;
    static constexpr uint8_t POS = 0b00000001;
    static constexpr uint8_t NEG = 0b00000010;
    /**
     * initialize the sensor
     */
    HMC5883L(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr} {
        // Config register 1: 1 sample averages, 75 Hz output rate
        writeReg(0x00, S1 | Hz75 | NORMAL);
        // Config register 2: set 1.3 Gauss
        writeReg(0x01, G1_3);
        // Mode register: cont operation mode
        writeReg(0x02, CONT);
    }

    /**
     * start async read of angle
     */
    void measure() {
        bus.trypush({
            .dev = this,
            .data = Buffer<uint8_t>(6),
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = 0x3,
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
        if (rq.data.size != 6) return;

        // data registers are ordered X, Z, Y
        this->Axes.x = rq.data.buf[1] | (rq.data.buf[0] << 8);
        this->Axes.z = rq.data.buf[3] | (rq.data.buf[2] << 8);
        this->Axes.y = rq.data.buf[5] | (rq.data.buf[4] << 8);

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

    struct {
        int16_t x, y, z;
    } Axes;

    double heading = 0;
    double declinationAngle = 0.0303; // http://www.magnetic-declination.com/
};
