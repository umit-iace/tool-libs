/** @file AS5048b.h
 *
 * Copyright (c) 2021 IACE
 */
#ifndef AS5048B_H
#define AS5048B_H

 // Default addresses for AS5048B
#define AS5048_ADDRESS 0x40 // 0b10000xx + ( A1 & A2 to GND)
#define AS5048B_PROG_REG 0x03
#define AS5048B_ADDR_REG 0x15
#define AS5048B_ZEROMSB_REG 0x16 //bits 0..7
#define AS5048B_ZEROLSB_REG 0x17 //bits 0..5
#define AS5048B_GAIN_REG 0xFA
#define AS5048B_DIAG_REG 0xFB
#define AS5048B_MAGNMSB_REG 0xFC //bits 0..7
#define AS5048B_MAGNLSB_REG 0xFD //bits 0..5
#define AS5048B_ANGLMSB_REG 0xFE //bits 0..7
#define AS5048B_ANGLLSB_REG 0xFF //bits 0..5
#define AS5048B_RESOLUTION 16384.0 //14 bits

#include <cstdint>
#include "stm/hal.h"
#include "stm/i2c.h"

 /**
  * Class describing a chain of AS5048B (i2c variant) Hall sensors
  */
class HallSensor {
private:
    ///\cond false

    uint8_t _chipAddress;


    uint16_t rawPos;
    double pos;



    void WriteReg(uint8_t reg, uint8_t val) {
        I2CRequest Req(
            _chipAddress,
            reg,
            &val,
            1,
            I2CRequest::I2C_MEM_WRITE,
            nullptr);
        HardwareI2C::master()->request(Req);
    }


    uint8_t ReadReg(uint8_t reg) {
        uint8_t readValue;
        I2CRequest read(
            ADDR_ACC,
            reg,
            (uint8_t*)readValue,
            1,
            I2CRequest::I2C_MEM_READ,
            nullptr);
        HardwareI2C::master()->request(read);
    }






    
    ///\endcond





public:
    /**
     * Constructor
     * @param chipAdress 0b10000xx with A1 and A2
     */
    HallSensor(uint8_t chipAddress)
    {
        _chipAddress = chipAddress;

        // get first measurement
        measure();
        // set initial angle to zero
        // setZero();

    }

    /**
     * @return measured sensor angle
     */
    double getAngle() {
        return pos;
    }

    /**
     * request measurement of sensor data and store the result
     */
    void measure() {
        
        uint16_t msbRaw = ReadReg(0xFE); // 8 bits of data: MSB
        uint16_t lsbRaw = ReadReg(0xFF); // 6 bits of data: LSB

        uint16_t fullRaw = lsbRaw + (msbRaw << 6);

        rawPos = fullRaw;
        pos = (double)fullRaw;

    }

    /**
     * zero out the sensor in hardware
     */
    void setZero() {

        WriteReg(0x16, (uint8_t)(rawPos >> 6)); // write MSB zero: shift LSB out of bounds
        WriteReg(0x17, (uint8_t)(rawPos & 0x3F)); // write LSB zero: mask first 6 bits

    }

};
#endif //AS5048_H
