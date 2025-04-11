/** @file M92.h
 *
 * Copyright (c) 2025 IACE
 */
#pragma once

#include "sys/i2c.h"
#include <cmath>

/**
 * Implementation of the M92 multi-chip 9-axis motion processor
 *
 * see
 * https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf
 * https://cdn.sparkfun.com/assets/learn_tutorials/5/5/0/MPU-9250-Register-Map.pdf
 * for details
 *
 * note: where accel & gyro go [x,y,z], compass goes [y,x,-z]
 */
struct M92 : I2C::Device {
    enum ADDR {
        DEFAULT = 0b1101000
        ALT = 0b1101001,
    };
    /// accelerometer full-scale in [g] @ 16bit
    enum ACCEL_FS { FS_2G, FS_4G, FS_8G, FS_16G, };
    double accel_lsb[] = {2./(1<<15), 4./(1<<15), 8./(1<<15), 16./(1<<15)};
    /// gyroscope full-scale in [degree per second] @ 16bit
    enum GYRO_FS { FS_250, FS_500, FS_1000, FS_2000, };
    double gyro_lsb[] = {250./(1<<15), 500./(1<<15), 1000./(1<<15), 2000./(1<<15)};
    /// MAG FS: +-4800microTesla
    /* enum MAG_RES { RES_14, RES_16 }; */
    /* double mag_lsb[] = {0.6, 15}; /// microTesla */

    M92(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : I2C::Device{bus, addr} {
    }
    void init() {
        write(PWR_MGMT_1, 0); // reset
        write(PWR_MGMT_1, 1); // select PLL clock
        write(CONFIG, 3); // set gyro delay to 5.9ms / bandwidth: 41Hz
        write(SMPLRT_DIV, 4); // sample rate: 200Hz
        set_gyro_fullscale(FS_1000);
        set_accel_fullscale(FS_4G);
        write(ACCEL_CONFIG+1, 3); //set accel delay to 11.8ms / bandwidth: 41Hz

        write(USER_CTRL, 0x20); // I2C Master
        write(I2C_MST_CTRL, 0x1D);
        write(I2C_MST_DELAY_CTRL, 0x81);
        write(I2C_SLV4_CTRL, 1);

        mag_write(CNTL, (1<<4)|3); // set mag to 16bit, 100Hz mode
        mag_enable_read();
    }

    void set_accel_fullscale(enum ACCEL_FS fs) {
        conf.accel = fs;
        write(ACCEL_CONFIG, (fs&3)<<3);
    }
    void set_gyro_fullscale(enum GYRO_FS fs) {
        conf.gyro = fs;
        write(GYRO_CONFIG, (fs&3)<<3);
    }

    void set_gyro_offset(int16_t x, int16_t y, int16_t z) {
        //TODO
    }
    void set_accel_offset(int16_t x, int16_t y, int16_t z) {
        //TODO
    }

    void measure() {
        bus.trypush({
            .dev = this,
            .data = 20,
            .opts = {
                .read = true,
                .mem = true,
            },
            .mem = ACCEL_X,
        });
    }



    void callback(const I2C::Request &rq) override {
        if (rq.opts.read != true) return;
        switch (rq.mem) {
        case ACCEL_X:
            accel.x = ((int16_t)rq.data[0] << 8) | rq.data[1];
            accel.y = ((int16_t)rq.data[2] << 8) | rq.data[3];
            accel.z = ((int16_t)rq.data[4] << 8) | rq.data[5];
            if (rq.data.len <= 6) return;
            temp   = ((int16_t)rq.data[6] << 8) | rq.data[7];
            gyro.x = ((int16_t)rq.data[8] << 8) | rq.data[9];
            gyro.y = ((int16_t)rq.data[10] << 8) | rq.data[11];
            gyro.z = ((int16_t)rq.data[12] << 8) | rq.data[13];
            mag.x  = ((int16_t)rq.data[14] << 8) | rq.data[15];
            mag.y  = ((int16_t)rq.data[16] << 8) | rq.data[17];
            mag.z  = ((int16_t)rq.data[18] << 8) | rq.data[19];
        }
    }

    Axis get_accel() {
        return {
            .x = accel.x * accel_lsb[conf.accel],
            .y = accel.y * accel_lsb[conf.accel],
            .z = accel.z * accel_lsb[conf.accel],
        };
    }
    Axis get_gyro() {
        return {
            .x = gyro.x * gyro_lsb[conf.gyro],
            .y = gyro.y * gyro_lsb[conf.gyro],
            .z = gyro.z * gyro_lsb[conf.gyro],
        };
    }
    /// get measurements in microTesla
    // TODO: make resolution configurable
    Axis get_mag() {
        return {
            .x = mag.x * 15,
            .y = mag.y * 15,
            .z = mag.z * 15,
        };
    }
    /// get measurement in °C
    double get_temp() {
        return temp / 333.87 + 21;
    }

    Axis {
        double x,y,z;
    };
    Raw {
        int16_t x,y,z;
    } accel {}, gyro {}, mag {};
    int16_t temp{};
private:
    struct {
        enum ACCEL_FS accel;
        enum GYRO_FS gyro;
    } conf {};
    enum REG_GYR_ACCEL {
        GYRO_OFF_X = 19,
        GYRO_OFF_Y = 21,
        GYRO_OFF_Z = 23,
        SMPLRT_DIV = 25,
        CONFIG = 26,
        GYRO_CONFIG = 27,
        ACCEL_CONFIG = 28,
        FIFO_EN = 35,
        I2C_MST_CTRL = 36,
        I2C_SLV0_ADDR = 37,
        I2C_SLV0_REG = 38,
        I2C_SLV0_DO = 99,
        I2C_SLV0_CTRL = 39,
        I2C_SLV4_CTRL = 52,
        ACCEL_X = 59,
        ACCEL_Y = 61,
        ACCEL_Z = 63,
        TEMP = 65,
        GYRO_X = 67,
        GYRO_Y = 69,
        GYRO_Z = 71,
        MAG_X = 73,
        MAG_Y = 75,
        MAG_Z = 77,
        USER_CTRL = 106,
        PWR_MGMT_1 = 107,
        PWR_MGMT_2 = 108,
        WHOAMI = 117, // should return 0x71
        ACCEL_OFF_X = 119,
        ACCEL_OFF_Y = 122,
        ACCEL_OFF_Z = 125,
    };
    enum REG_MAG {
        WIA = 0, // should return 0x48
        DATA = 3, // x, y, z 2byte each
        ST2 = 9, // read after each measurement
        CNTL = 10,
    };
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
    void mag_write(uint8_t addr, uint8_t val) {
        write(I2C_SLV0_ADDR, 0xc);
        write(I2C_SLV0_REG, addr);
        write(I2C_SLV0_DO, val);
        write(I2C_SLV0_CTRL, (1<<5) | 1);
    }
    void mag_enable_read() {
        write(I2C_SLV0_ADDR, 0x8c);
        write(I2C_SLV0_REG, DATA);
        write(I2C_SLV0_CTRL, 0x87 | (1 << 6) | (1 << 4)); // enable read 7 byte,
                                    // auto-convert from Little to Big Endian
    }
};
