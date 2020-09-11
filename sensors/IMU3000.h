/** @file IMU3000.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef IMU3000_H
#define IMU3000_H

#include <cstdint>
#include "stm/hal.h"
#include "stm/i2c.h"

///\cond false
#define ADDR_GYRO       (0b1101000)
#define ADDR_ACC        (0x53)
///\endcond

class IMU3000 {
public:
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
     * initialize the accelerometer and the gyro
     */
    IMU3000(enum acc_range ar=ACC_16G, bool afr=true,
            enum gyro_range gr=GYRO_250, uint8_t gfilt=1, uint8_t gdiv=4) {
        setAccRange(ar, afr);
        accWriteReg(0x2D, 1 << 3); // enable acceleration measurements
        setGyroConf(gr, gfilt, gdiv);
    }

    /**
     * start async read of acceleration and gyro data from sensor
     */
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
            gyro[i] = gyroData[i] / gyroFactor * 3.14159265358979 / 180;
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
    ///\cond false
    int16_t gyroData[3] = {};
    int16_t accData[3] = {};

    double accel[3] = {}, gyro[3] = {};
    double accelFactor = 0, gyroFactor = 0;

    void readGyroData() {
        I2CRequest gyroread (
                ADDR_GYRO,
                0x1D,
                (uint8_t *)gyroData,
                6,
                I2CRequest::I2C_MEM_READ,
                flip3HW);
        HardwareI2C::master()->request(gyroread);
    }

    void readAccData() {
        I2CRequest accread (
                ADDR_ACC,
                0x32,
                (uint8_t *)accData,
                6,
                I2CRequest::I2C_MEM_READ,
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

    void gyroWriteReg(uint8_t reg, uint8_t val) {
        I2CRequest gyroReq (
                ADDR_GYRO,
                reg,
                &val,
                1,
                I2CRequest::I2C_MEM_WRITE,
                nullptr);
        HardwareI2C::master()->request(gyroReq);
    }

    void accWriteReg(uint8_t reg, uint8_t val) {
        I2CRequest accReq (
                ADDR_ACC,
                reg,
                &val,
                1,
                I2CRequest::I2C_MEM_WRITE,
                nullptr);
        HardwareI2C::master()->request(accReq);
    }

    ///\endcond
};

#endif //IMU3000_H
