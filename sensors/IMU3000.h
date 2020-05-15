/** @file IMU3000.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef IMU3000_H
#define IMU3000_H

#include "stm/i2c.h"

#define ADDR_GYRO       (0b1101000)
#define ADDR_ACC        (0x53)

class IMU3000 {
public:
    IMU3000() {
        this->init();
    }

    void measure() {
        readAccData();
        readGyroData();
    }

    /**
     * get angular velocity in rad/s
     * @return pointer to 3 doubles x,y,z
     */
    double *getGyro() {
        for (int i = 0; i < 3; ++i) {
            gyro[i] = gyroData[i] / gyroFactor * 3.14159265 / 180;
        }
        return gyro;
    }

    /**
     * get acceleration values in m/s^2
     * @return pointer to 3 doubles x,y,z
     */
    double *getAcc() {
        for (int i = 0; i < 3; ++i) {
            accel[i] = accData[i] * accelFactor;
        }
        return accel;
    }

private:
    int16_t gyroData[3] = {};
    int16_t accData[3] = {};

    double accel[3] = {}, gyro[3] = {};
    double accelFactor = 0, gyroFactor = 0;

    void readGyroData() {
        I2CRequest gyroread (
                ADDR_GYRO,
                I2CRequest::READ,
                0x1D,
                (uint8_t *)gyroData,
                6,
                flip3HW);
        HardwareI2C::master()->request(gyroread);
    }

    void readAccData() {
        I2CRequest accread (
                ADDR_ACC,
                I2CRequest::READ,
                0x32,
                (uint8_t *)accData,
                6,
                nullptr);
        HardwareI2C::master()->request(accread);
    }

    /**
     * flip endianness of 3 uint16_t in a row
     *
     * needed for gyro data.
     * @param data
     */
    static void flip3HW(uint8_t *data) {
        for (int i = 0; i < 3; ++i) {
            ((int16_t *)data)[i] = data[2*i] << 8 | data[2*i+1];
        }
    }

    /**
     * possible ranges for accelerometer
     */
    enum acc_range {
        ACC_2G = 0,
        ACC_4G = 1,
        ACC_8G = 2,
        ACC_16G = 3
    };

    /**
     * set the range and correct factor for accelerometer
     * @param range value of acc_range
     * @param fullres boolean to enable full range
     */
    void setAccRange(enum acc_range range, bool fullres) {
        if (fullres) {
            accelFactor = 0.004 * 9.81;
        } else {
            accelFactor = (2 << range) * 9.81 / 1024.;
        }
        accWriteReg(0x31, fullres << 3 | range);
    }

    /**
     * possible ranges for gyro
     *
     * measured in degrees/sec.
     */
    enum gyro_range {
        GYRO_250 = 0,
        GYRO_500 = 1,
        GYRO_1000 = 2,
        GYRO_2000 = 3
    };

    /**
     * configure Gyro settings
     * @param range full scale range
     * @param filt digital low pass filter
     * @param div sample rate divider
     */
    void setGyroConf(enum gyro_range range, uint8_t filt, uint8_t div) {
        gyroFactor = 131. / (1 << range);

        gyroWriteReg(0x16, range << 3 | filt);
        gyroWriteReg(0x15, div); // gyro sample rate
        gyroWriteReg(0x3E, 1); // clk sel
        gyroWriteReg(0x3D, 1); // rst gyro
    }

    /**
     * initialize the accelerometer and the gyro
     */
    void init() {
        setAccRange(ACC_16G, true);
        accWriteReg(0x2D, 1 << 3); // enable acceleration measurements
        setGyroConf(GYRO_250, 1, 4);
    }

    void gyroWriteReg(uint8_t reg, uint8_t val) {
        I2CRequest gyroReq (
                ADDR_GYRO,
                I2CRequest::WRITE,
                reg,
                &val,
                1,
                nullptr);
        HardwareI2C::master()->request(gyroReq);
    }

    void accWriteReg(uint8_t reg, uint8_t val) {
        I2CRequest accReq (
                ADDR_ACC,
                I2CRequest::WRITE,
                reg,
                &val,
                1,
                nullptr);
        HardwareI2C::master()->request(accReq);
    }
};

#endif //IMU3000_H
