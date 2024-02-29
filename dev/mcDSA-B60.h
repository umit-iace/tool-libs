/** @file mcDSA-B60.h
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
            .type = CAN::Open::RPDO::CHANGE,
            .map = {{.ix = 0x3300, .sub = 0, .len = 32}},
            };
    CAN::Open::TPDO msrPDO = {
            .N = 1,
            .type = CAN::Open::TPDO::SYNC,
            .map = {
                {.ix = 0x3a04, .sub = 1, .len = 32}, // speed
                {.ix = 0x3262, .sub = 1, .len = 32}, // current [mA]
            },
    };

    mcDSA(CAN::Open::Dispatch &out, uint8_t id, bool invert) : CAN::Open::Device(out, id) {
        enable(false);
        init(invert);
        enablepdo(speedPDO);
        enablepdo(msrPDO);
        enable(true);
    }
    struct Measurements {
        double speed, speedDes, current, position;
        uint32_t error;
    } meas;

    /** Enable or Disable the motor driver */
    void enable(bool en) {
        w8(0x3004, 0, en);
    }

    //TODO:
    /** Set the motor speed in u/min.
     * Capped at +- 1m/s
     */
    void speed(double speed) {
        /* speed = fmin(1, fmax(-1, speed)); */
        /* int iSpeed = (int) (6000 / M_PI / CAR_WHEEL_DIAMETER * dSpeed); // calculates from m/s to (U/100) / min */
        /* int iSpeed; */
        wpdo(speedPDO, speed);
    }
    double speed() {
        return meas.speed;
    }

    /**
     * Read current motor data.
     *
     * Includes actual speed, desired speed, motor current,
     * motor position (angle), motor error.
     *
     * Data can be accessed in @ref tData
     */
    void readData() {
        read(0x3a04, 1);//, &this->tData.tSpeed);
        read(0x3361, 0);//, &this->tData.tSpeedDes);
        read(0x3262, 1);//, &this->tData.tCurrent);
        read(0x394a, 0);//, &this->tData.tPosition);
        read(0x3001, 0);//, &this->tData.tError);
    }

    /**
     * Return the current motor speed in m/s
     */
    double getSpeed() {
        return{};
        /* return ((int) this->tData.tSpeed.u32) * M_PI * CAR_WHEEL_DIAMETER / 6000;       // calculates from (U/100)/min to m/s */
    }

    /**
     * Return the current desired motor speed in m/s
     */
    double getSpeedDes() {
        return{};
        /* return ((int) this->tData.tSpeedDes.u32) * M_PI * CAR_WHEEL_DIAMETER / 6000;       // calculates from (U/100)/min to m/s */
    }

    /**
     * Return the current motor current in mA
     */
    double getCurrent() {
        return{};
        /* return ((int) this->tData.tCurrent.u32); */
    }

    void callback(CAN::Open::SDO rq) {
        switch(rq.ix) {
        case 0x3a04: assert(rq.sub == 1); meas.speed = rq.data; break;
        case 0x3361: assert(rq.sub == 0); meas.speedDes = rq.data; break;
        case 0x3262: assert(rq.sub == 1); meas.current = rq.data; break;
        case 0x394a: assert(rq.sub == 0); meas.position = rq.data; break;
        case 0x3001: assert(rq.sub == 0); meas.error = rq.data; break;
        }
    }
    void callback(CAN::Open::TPDO rq) {
        uint64_t raw = rq.data;
        double speed = ((int32_t)raw);
        double current = (int32_t)(rq.data >> 32) * 0.001;
    }

    //TODO: this needs to happen on a higher level: heartbeat
    //   out.push({.data = 0x5, .id = 0x701, .opts = {.dlc = 1}}); // send heartbeat

    void init(bool invert) {
		w8(0x3000, 0, 1); // clear errors
		//w32(0x3000, 0, 0x82); // reset to defaults

        // motor config
		w8(0x3900, 0, 1); // motor type: BLDC
		w16(0x3902, 0, 24000); // nominal supply voltage
		w8(0x3910, 0, 8); // number of poles
		w8(0x3911, 0, 1 << (invert ? 0 : 1)); // in case direction needs changing
		w32(0x3221, 0, 1785); // max positive current
		w32(0x3223, 0, 1785); // max negative current

        w16(0x3a02, 0, 5); // velocity measure time [ms]

        // current controller
		w16(0x3250, 0, 0x113); // current feedback through internal sensor
		w16(0x3210, 0, 200); // Kp
		w16(0x3211, 0, 100); // Ki
		w16(0x3219, 1, 256); // Kdiv

        // svel controller settings
		w32(0x3550, 0, 0x94a); // svel feedback with hall sensors
		w16(0x3510, 0, 10); // Kp
		w16(0x3511, 0, 10); // Ki
		w16(0x35a1, 0, 10000); // svel max drehzahlbereich

        // vel controller settings
		w16(0x3310, 0, 0); // Kp
		w16(0x3311, 0, 0); // Ki
		w16(0x3312, 0, 0); // Kd
		w16(0x3314, 0, 1000); // feed forward in o/oo
		w16(0x3315, 0, 0); // acceleration feed forward
		w32(0x3350, 0, 0x94a); // velocity feed back through HALL sensors

        // vel settings
		w16(0x33a0, 0, 1000); // sampling time in us
		w8(0x334c, 0, 1); // turn on ramp generator
		w32(0x3340, 0, 1000); // ramp generator: vel_acc_dv
		w32(0x3341, 0, 1000); // ramp generator: vel_acc_dt
		w32(0x3342, 0, 1000); // ramp generator: vel_dec_dv
		w32(0x3343, 0, 1000); // ramp generator: vel_dec_dt

        // factors
		w8(0x3b00, 0, 3); // activate factors
		w8(0x3b00, 1, 1); // high precision
		w32(0x3302, 0, 1); // Fvscale zähler
		w32(0x3303, 0, 100); // Fvscale nenner
		w32(0x330A, 0, 878); // Fvdim zähler
		w32(0x330B, 0, 10); // Fvdim nenner
		w32(0x3B19, 0, 878); // gear motor numerator
		w32(0x3B19, 1, 10); // gear motor denominator

        //heartbeat settings
		/* w32(0x1016, 1, (1<<16) | ((int)EXP_DT)); // heartbeat guarding (node | time [ms]) */
		w8(0x1029, 1, 0); // reaction to error
        // motor controller startup settings
		w8(0x3003, 0, 3); // motor driver mode
    }
};
