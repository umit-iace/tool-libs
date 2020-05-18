/** @file ADS1115.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef ADS1115_H
#define ADS1115_H

#include "stm/hal.h"
#include "stm/i2c.h"

/**
 * Implementation of the ADS1115 analog to digital converter.
 *
 * @warning comparator modes not implemented!
 *
 * @warning threshold settings not implemented!
 *
 * @warning single shot conversion mode not implemented!
 *
 * see
 * http://www.ti.com/lit/ds/symlink/ads1115.pdf
 * for details
 */
class ADS1115 {
private:
    ///\cond false
    enum {
        CONVERSION,
        CONFIG,
        LO_THRESH,
        HI_THRESH
    };

    const double uV[6] = {
        187.5,
        125,
        62.5,
        31.25,
        15.625,
        7.8125
    };
    ///\endcond
public:
    /// full scale range options
    enum range {
        FS6144,
        FS4096,
        FS2048,
        FS1024,
        FS0512,
        FS0256
    } eFullScale = FS2048;      ///< current full scale setting

    /// output data rate (samples per second) options
    enum dataRate {
        SPS8,
        SPS16,
        SPS32,
        SPS64,
        SPS128,
        SPS250,
        SPS475,
        SPS860
    } eDataRate = SPS128;       ///< current data rate

    /** multiplexer setting options
     *
     * first index is positive lead, second index negative
     *
     * only one index means single ended measurements
     *
     * e.g. AIN0_3 differentially measures voltage on pin 0 against pin 3
     */
    enum mux {
        AIN0_1,
        AIN0_3,
        AIN1_3,
        AIN2_3,
        AIN0,
        AIN1,
        AIN2,
        AIN3
    } eMux = AIN0_1;            ///< current multiplexer setting

    ///\cond false
    // single conversion mode
    bool single = false;
    bool cMod = 0;
    bool cPol = 0;
    bool cLat = 0;
    uint8_t cQue = 0;
    ///\endcond

    /**
     * configure and start voltage conversions
     * @param address 7bit I2C address of ADS1115 device
     * @param fs full scale range on inputs
     * @param dr data rate / conversion speed
     * @param mx initial multiplexer setting
     */
    ADS1115(uint8_t address = 0b1001000, enum range fs = FS6144,
            enum dataRate dr = SPS64, enum mux mx = AIN0):
                eFullScale(fs), eDataRate(dr), eMux(mx) {
        updateConfig();
    }

    /**
     * update device config
     *
     * sends parameters set in class to device
     */
    void updateConfig() {
        write(CONFIG, 0 << 15 |
                eMux << 12 |
                eFullScale << 9 |
                (single & 1) << 8 |
                eDataRate << 5 |
                (cMod & 1) << 4 |
                (cPol & 1) << 3 |
                (cLat & 1) << 2 |
                cQue
                );
        setPointer(CONVERSION);
    }

    /**
     * async read data from device
     */
    void measure() {
        read();
    }

    /**
     * @return last measured voltage [V]
     */
    double volts() {
        return *(uint16_t *)buffer * uV[eFullScale] * 1e-6;
    }

private:
    ///\cond false
    uint8_t address;
    uint8_t buffer[2];

    void setPointer(uint8_t reg) {
        I2CRequest point (
                address,
                I2CRequest::WRITE,
                0,
                &reg,
                1,
                process,
                nullptr);
        HardwareI2C::master()->request(point);
    }

    void read() {
        I2CRequest meas (
                address,
                I2CRequest::READ,
                0,
                buffer,
                2,
                process,
                nullptr);
        HardwareI2C::master()->request(meas);
    }

    void write(uint8_t reg, uint16_t val) {
        uint8_t data[3] = {reg, (uint8_t)(val<<8), (uint8_t)val};
        I2CRequest write (
                address,
                I2CRequest::WRITE,
                0,
                data,
                3,
                process,
                nullptr);
        HardwareI2C::master()->request(write);
    }

    static void process(I2CRequest &rq, I2C_HandleTypeDef *hI2C) {
        switch(rq.dir) {
        case I2CRequest::READ:
            HAL_I2C_Master_Receive_IT(hI2C, rq.address << 1, rq.pData, rq.dataLen);
            break;
        case I2CRequest::WRITE:
            HAL_I2C_Master_Transmit_IT(hI2C, rq.address << 1, rq.pData, rq.dataLen);
            break;
        }
    }
    ///\endcond
};
#endif //ADS1115_H
