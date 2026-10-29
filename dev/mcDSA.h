/** @file mcDSA.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cmath>
#include <comm/canopen.h>

/**
 * @brief Driver for the mcDSA-B60 CANOpen Motor driver
 */
struct mcDSA : CAN::Open::Device {
    CAN::Open::RPDO speedPDO = {
            .N = 1,
            .type = CAN::Open::PDOType::CHANGE,
            .map = {{.ix = 0x3300, .sub = 0, .len = 32}},
            };
    CAN::Open::TPDO msrPDO = {
            .N = 1,
            .type = CAN::Open::PDOType::SYNC,
            .map = {
                {.ix = 0x3a04, .sub = 1, .len = 32}, // speed
                {.ix = 0x3262, .sub = 1, .len = 32}, // current [mA]
            },
    };

    mcDSA(CAN::Open::Dispatch &out, uint8_t id) : CAN::Open::Device(out, id) {
        enable(false);
        enablePDO(speedPDO);
        enablePDO(msrPDO);
        enable(true);
    }
    struct Measurements {
        double speed, speedDes, current, position;
        uint32_t error;
    } meas;

    /** Enable or Disable the motor driver */
    void enable(bool en=true) {
        w8(0x3004, 0, en);
    }
    void disable() {
        enable(false);
    }
    void clear() {
        w8(0x3000, 0, 1); // clear errors
    }

    //TODO:
    /** Set the motor speed in u/min.
     * Only works if device is in Operational State
     * Capped at +- 1m/s
     */
    void speed(double speed) {
        speed = fmin(1, fmax(-1, speed));
        speedPDO.map[0].data = 6000 / M_PI * speed;
        wPDO(speedPDO);
    }
    double speed() {
        return msrPDO.map[0].data;
    }

    /**
     * Read current motor data.
     *
     * Includes actual speed, desired speed, motor current,
     * motor position (angle), motor error.
     */
    void readData() {
        read(0x3a04, 1); // speed
        read(0x3361, 0); // desired speed
        read(0x3262, 1); // current
        read(0x394a, 0); // position
        read(0x3001, 0); // error
    }

    /**
     * Return the current motor current in mA
     */
    double current() {
        return msrPDO.map[1].data;
    }

    void callback(CAN::Open::SDO rq) override {
        // measurements are signed 32 bit values
        int32_t val = (int32_t)rq.data;
        switch(rq.ix) {
        case 0x3a04: assert(rq.sub == 1); meas.speed = val; break;
        case 0x3361: assert(rq.sub == 0); meas.speedDes = val; break;
        case 0x3262: assert(rq.sub == 1); meas.current = val; break;
        case 0x394a: assert(rq.sub == 0); meas.position = val; break;
        case 0x3001: assert(rq.sub == 0); meas.error = rq.data; break;
        }
    }
    void callback(CAN::Open::TPDO) override {
    }
};
