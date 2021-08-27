/** @file AS5145.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef AS5145_H
#define AS5145_H

#include "stm/spi.h"

/**
 * Class describing a chain of AS5145 Hall sensors
 */
class AS5145 : ChipSelect {
private:
    ///\cond false
    // sensor data struct. lsb to msb order.
    union SensorData {
        struct __packed  {
            uint8_t EVEN: 1;
            uint8_t DEC: 1;
            uint8_t INC: 1;
            uint8_t LIN: 1;
            uint8_t COF: 1;
            uint8_t OCF: 1;
            uint16_t POS: 12;
            uint8_t : 1;
        };
        uint32_t u32;
    } *sensor = nullptr;
    const unsigned short NUMBITS = 19;  // number of bits in sensor data struct
    uint8_t *buffer = nullptr;          // temporary data buffer
    const unsigned int BUFLEN;          // needed length of buffer to store data from all sensors
    int num = 0;                        // number of sensors attached to daisy-chain
    ///\endcond

public:
    /**
     * Constructor
     * @param pin GPIO pin of chip select line
     * @param port GPIO port of chip select line
     * @param n     number of sensors attached to daisy-chain
     */
    AS5145(uint32_t pin, GPIO_TypeDef *port, int n) : ChipSelect(pin, port),
            BUFLEN((n * NUMBITS) / 8UL + (n * NUMBITS % 8 ? 1 : 0)) // calculate bytes needed for n sensors
            {
        buffer = new uint8_t[BUFLEN]();
        sensor = new SensorData[n]();
        num = n;
    }

    /**
     * @param i index of sensor in chain
     * @return measured sensor angle
     */
    double getAngle(int i = 0) {
        return sensor[i].POS;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        HardwareSPI::master()->request(new SPIRequest(
                this,
                SPIRequest::MISO,
                nullptr,
                buffer,
                BUFLEN,
                (void *)true)
        );
    }

    ///\cond false
    /**
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(void *cbData) override {
        flip(buffer, BUFLEN);

        for (int i = 0; i < num; ++i) {
            uint8_t off = (i+1)*NUMBITS;
            sensor[i].u32 = *(uint32_t *)&buffer[BUFLEN-1 - off/8] >> (8 - off%8);
        }
//        bitwisecopy((uint8_t *)sensor, NUMBITS, sizeof(*sensor), buffer, num);
    }
    ///\endcond
};
#endif //AS5145_H
