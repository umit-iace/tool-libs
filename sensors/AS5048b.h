/** @file IMU3000.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef AS5048B_H
#define AS5048B_H

#include <cstdint>
#include "stm/hal.h"
#include "stm/i2c.h"

///\cond false
#define AS5048B_ADDR          (0x40 << 1)
#define AS5048B_REG_ANGLE_LSB (0xFE)
#define AS5048B_REG_ANGLE_MSB (0xFF)
///\endcond

class AS5048B {
public:

    /**
     * initialize the sensor
     */
    AS5048B(HardwareI2C *i2cinstance, bool A0, bool A1) {
        address = AS5048B_ADDR || ((A0 ? 1 : 0) << 0) || ((A1 ? 1 : 0) << 1);
        instance = i2cinstance;
    }

    /**
     * start async read of angle
     */
    void measure() {
        readUpperAngleData();
        readLowerAngleData();
    }

    /**
     * get angular position values in degrees
     * @return value in rotational degrees
     */
    double getAngle() {
        uint8_t upperbyte = upperData[0];
        uint8_t lowerbyte = lowerData[0];

        // this code is based on an arduino library for the AS5048B: https://github.com/sosandroid/AMS_AS5048B
        // the datasheet of AS5048B mentions other bit patterns, but this seems to work fine, don't know why
        uint16_t rawangle = ((uint16_t)upperbyte << 6);
        rawangle += (lowerbyte & 0x3F);

        // the length of rawangle is 8+6 bit, so a total of 16384 in decimal
        float angle = ((float)rawangle / 16384) * 360;

        if ((Angle_lastangle - angle) > 180)
        {
            // positive wrap around
            Angle_wraps +=1;
        }
        else if ((Angle_lastangle - angle) < -180)
        {
            // negative wrap around
            Angle_wraps -=1;
        }
        else
        {
            // no wrap occurance
        }
        Angle_current = Angle_wraps * 360 + angle;
        Angle_lastangle = angle;
        return Angle_current;
    }


private:
    ///\cond false
	
	uint8_t address = AS5048B_ADDR;

    HardwareI2C *instance;
	
    uint8_t upperData[2] = {};
    uint8_t lowerData[2] = {};
	
	float Angle_current = 0;		// angle for rotational movement
	int8_t Angle_wraps = 0;			// number of full turns
	float Angle_lastangle = 180;	// last angle to determine the direction of rotational movement

    void readUpperAngleData() {
        I2CRequest angleread (
                address,
                AS5048B_REG_ANGLE_MSB,
                (uint8_t *)upperData,
                1,
                I2CRequest::I2C_MEM_READ,
                nullptr);
        instance->request(angleread);
    }

    void readLowerAngleData() {
        I2CRequest angleread (
                address,
                AS5048B_REG_ANGLE_LSB,
                (uint8_t *)lowerData,
                1,
                I2CRequest::I2C_MEM_READ,
                nullptr);
        instance->request(angleread);
    }


    ///\endcond
};

#endif //AS5048B_H
