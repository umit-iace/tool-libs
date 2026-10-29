/** @file BMI160.h
 *
 * Copyright (c) 2022 IACE
 */
#pragma once

#include <cstdint>
#include <cstring>
#include <core/kern.h>
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
        writeReg(CMD, 0x11);   // enable accelerometer
        since = k.time;
        powerup = k.every(1, *this, &BMI160::startup);
    }
    ~BMI160() {
        if (powerup) k.remove(powerup);
    }
    BMI160(const BMI160 &) = delete;
    BMI160& operator=(const BMI160 &) = delete;
    /** true once accelerometer & gyroscope are up and running */
    bool ready() const { return state == READY; }

    /**
     * start async read-out of sensor data
     */
    void measure() {
        bus.trypush({
            .dev = this,
            .data = 12,
            .opts = {
                .read = true,
                .mem = true,
                },
            .mem = 0xc, // gyro data, followed by acc data
            });
    }

    Axes acc{}, gyro{};
private:
    static constexpr uint8_t CMD = 0x7e;
    enum { ACC_STARTING, GYRO_STARTING, READY } state{};
    uint32_t since{};
    Schedule::Schedulable *powerup{};
    void startup(uint32_t now, uint32_t) {
        switch (state) {
        case ACC_STARTING:
            // the next command has to wait until this one is done (3.8ms)
            if (now - since < 5) break;
            writeReg(CMD, 0x15); // enable gyroscope
            state = GYRO_STARTING;
            since = now;
            break;
        case GYRO_STARTING:
            if (now - since < 81) break; // gyro startup time 80ms
            state = READY;
            k.remove(powerup); // destroys this Schedulable, nothing after it
            powerup = nullptr;
            return;
        case READY:
            break;
        }
    }
public:
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
        if (rq.data.size != 12 || rq.data.len != 12) return;
        int16_t view[6]; // little endian, like the target
        memcpy(view, rq.data.buf, sizeof view);
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
