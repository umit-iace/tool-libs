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
    RequestQueue<SPIRequest> *bus;
    // sensor data struct. lsb to msb order.
    struct __packed SensorData {
        uint8_t EVEN:1;
        uint8_t DEC:1;
        uint8_t INC:1;
        uint8_t LIN:1;
        uint8_t COF:1;
        uint8_t OCF:1;
        int16_t POS:12;
        uint8_t :1;
    } *sensor = nullptr;
    const unsigned short NUMBITS = 19;  // number of bits in sensor data struct
    uint8_t *buffer = nullptr;          // temporary data buffer
    const unsigned int BUFLEN;          // needed length of buffer to store data from all sensors
    int num = 0;                        // number of sensors attached to daisy-chain
    ///\endcond

public:
    /**
     * Constructor
     * @param bus SPI bus
     * @param cs ChipSelect pin
     * @param n number of sensors attached to daisy-chain
     */
    AS5145(RequestQueue<SPIRequest> *bus, DIO cs, int n) : ChipSelect(cs),
            bus(bus),
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
    int16_t getVal(int i = 0) {
        return sensor[i].POS;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        bus->request(new SPIRequest{
                this,
                SPIRequest::MISO,
                nullptr,
                buffer,
                BUFLEN,
                (void *)true}
        );
    }

    ///\cond false
    /**
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(void *cbData) override {
        bitwisecopy((uint8_t *)sensor, buffer, NUMBITS, num);
    }
    ///\endcond
};
#endif //AS5145_H
