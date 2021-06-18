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
    class Gyro : I2CDevice {
    public:
        /**
         * get angular velocity in rad/s
         * @return pointer to 3 doubles x,y,z
         */
        double *get() {
            return angvel;
        }

        /**
         * possible ranges for gyro
         *
         * measured in degrees/sec.
         */
        enum range {
            RANGE_250 = 0,
            RANGE_500 = 1,
            RANGE_1000 = 2,
            RANGE_2000 = 3,
        };

        /**
         * configure Gyro settings
         * @param range full scale range
         * @param filt digital low pass filter
         * @param div sample rate divider
         */
        void setConf(enum range range, uint8_t filt, uint8_t div) {
            this->factor = 131. / (1 << range);

            this->writeReg(0x16, range << 3 | filt);
            this->writeReg(0x15, div); // gyro sample rate
            this->writeReg(0x3E, 1); // clk sel
            this->writeReg(0x3D, 1); // rst gyro
        }

        /**
         * start async read of gyro data from sensor
         */
        void measure() {
            HardwareI2C::master()->request(new I2CRequest (
                    this,
                    0x1D,
                    (uint8_t *)this->raw,
                    6,
                    I2CRequest::I2C_MEM_READ,
                    (void *)true)
            );
        }

        /**
         * @param gr gyroscope range
         * @param gfilt digital low pass filter
         * @param gdiv sample rate divider
         */
        Gyro(enum range gr, uint8_t gfilt, uint8_t gdiv) : I2CDevice(ADDR_GYRO) {
            setConf(gr, gfilt, gdiv);
        }

    protected:
        int16_t raw[3] = {};
        double angvel[3] = {};
        double factor = 1;

        void writeReg(uint8_t reg, uint8_t val) {
            HardwareI2C::master()->request(new I2CRequest(
                    this,
                    reg,
                    &val,
                    1,
                    I2CRequest::I2C_MEM_WRITE,
                    nullptr)
            );
        }

        void callback(void *cbData) override {
            for (int i = 0; i < 3; ++i) {
                // calculate angle velocities from data with flipped endianness
                angvel[i] = (int16_t)(raw[i] << 8 | raw[i] >> 8) / factor * M_PI / 180;
            }
        }
    };

    class Acc : I2CDevice {
    public:
        /**
         * get acceleration values in m/s^2
         * @return pointer to 3 doubles x,y,z
         */
        double *get() {
            return accel;
        }

        /**
         * possible ranges for accelerometer
         */
        enum range {
            RANGE_2G = 0,
            RANGE_4G = 1,
            RANGE_8G = 2,
            RANGE_16G = 3,
        };

        /**
         * set the range and correct factor for accelerometer
         * @param ar accelerometer range
         * @param fullres boolean to enable full range
         */
        void setConf(enum range ar, bool fullres) {
            if (fullres) {
                factor = 0.004 * 9.81;
            } else {
                factor = (2 << ar) * 9.81 / 1024.;
            }
            this->writeReg(0x31, fullres << 3 | ar);
        }

        /**
         * start async read of acceleration data from sensor
         */
        void measure() {
            HardwareI2C::master()->request(new I2CRequest(
                    this,
                    0x32,
                    (uint8_t *)this->raw,
                    6,
                    I2CRequest::I2C_MEM_READ,
                    (void *)true)
            );
        }

        /**
         * @param ar accelerometer range
         * @param afr bool full range
         */
        Acc(enum range ar, bool afr) : I2CDevice(ADDR_ACC) {
            this->setConf(ar, afr);
            // enable acceleration measurements
            this->writeReg(0x2D, 1 << 3);
        }

    protected:
        int16_t raw[3] = {};
        double accel[3] = {};
        double factor = 1;

        void writeReg(uint8_t reg, uint8_t val) {
            HardwareI2C::master()->request(new I2CRequest(
                    this,
                    reg,
                    &val,
                    1,
                    I2CRequest::I2C_MEM_WRITE,
                    nullptr)
            );
        }

        void callback(void *cbData) override {
            for (int i = 0; i < 3; ++i) {
                accel[i] = raw[i] * factor;
            }
        }
    };

public:
    Acc acc;
    Gyro gyro;

    /**
     * initialize the accelerometer and the gyro
     */
    IMU3000(enum Acc::range ar=Acc::RANGE_16G, bool afr=true,
            enum Gyro::range gr=Gyro::RANGE_250, uint8_t gfilt=1, uint8_t gdiv=4) :
                acc(ar, afr), gyro(gr, gfilt, gdiv) { }

    /**
     * start async read of acceleration and gyro data from sensor
     */
    void measure() {
        acc.measure();
        gyro.measure();
    }
};

#endif //IMU3000_H
