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
 * https://github.com/sparkfun/Qwiic_IMU_BNO080/blob/master/Documents/SH-2-Reference-Manual-v1.2.pdf
 * for details
 *
 */

struct BNO085 : I2C::Device {
    enum ADDR {
        DEFAULT = 0b1001010,
        ALT = 0b1001011,
    };

    BNO085(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr}, bInit(false) {
    }
    void init() {
        write(this->address, 32);

        // set features
        for (char i = 0; i < 21; ++i) {
            write(this->address, quadSetup[i]);
        }
    }

    void measure() {
        bus.trypush({
            .dev = this,
            .data = 20,
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = 21,
        });
    }

    void callback(const I2C::Request &rq) override {
        if (this->bInit) {
            this->bInit = true;
        }{
            auto q1 = (((int16_t)rq.data.buf[14] << 8) | rq.data.buf[13] );
            auto q2 = (((int16_t)rq.data.buf[16] << 8) | rq.data.buf[15] );
            auto q3 = (((int16_t)rq.data.buf[18] << 8) | rq.data.buf[17] );
            auto q0 = (((int16_t)rq.data.buf[20] << 8) | rq.data.buf[19] );
        }
    }

private:
    bool bInit = false;

    int reporting_frequency    = 400;                               // reporting frequency in Hz
    uint32_t rate              = 1000000 / reporting_frequency;
    uint8_t B0_rate            = rate & 0xFF;                       //calculates LSB (byte 0)
    uint8_t B1_rate            = (rate >> 8) & 0xFF;
    uint8_t B2_rate            = (rate >> 16) & 0xFF;
    uint8_t B3_rate            = rate >> 24;
    uint8_t quadSetup[21] = {
        21,
        0,
        2,
        0,
        0xFD,
        0x05,   // defines kind of rotation vector (0x05), geomagnetic (0x09), AR/VR (0x28)
        0,
        0,
        0,
        B0_rate,
        B1_rate,
        B2_rate,
        B3_rate,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };

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
