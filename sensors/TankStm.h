/** @file TankStm.h
 *
 * Copyright (c) 2021 IACE
 */
#ifndef TANKSTM_H
#define TANKSTM_H

#include "stm/spi.h"

/**
 * Class describing a chain of tanks
 */
class TankStm : public ChipSelect {
private:
    /// sensor data struct. lsb to msb order.
    struct __packed TankData {
        uint32_t T1HEIGHT:1;
        uint32_t T2HEIGHT:1;
    } *sensor = nullptr;
    const unsigned short NUMBITS = 64;  // number of bits in sensor data struct
    uint8_t *buffer = nullptr;          // temporary data buffer
    const unsigned int BUFLEN;          // needed length of buffer to store data from all sensors
    int num = 0;                        // number of sensors attached to daisy-chain

public:
    /**
     * Constructor
     * @param pin GPIO pin of chip select line
     * @param port GPIO port of chip select line
     * @param n     number of sensors attached to daisy-chain
     */
    TankStm(uint32_t pin, GPIO_TypeDef *port, int n) :
        ChipSelect(pin, port), BUFLEN((n * NUMBITS) / 8UL + (n * NUMBITS % 8 ? 1 : 0)) // calculate bytes needed for n sensors
    {
        buffer = new uint8_t[BUFLEN]();
        sensor = new TankData[n]();
        num = n;
    }

    /**
     * return currently measured height of tank 1
     * @param i index of sensor in chain
     * @return height
     */
    double getT1Height(int i = 0) {
        return sensor[i].T1HEIGHT;
    }

    /**
     * return currently measured height of tank 2
     * @param i index of sensor in chain
     * @return height
     */
    double getT2Height(int i = 0) {
        return sensor[i].T2HEIGHT;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        SPIRequest r = {this, SPIRequest::MISO, nullptr, buffer, BUFLEN, (void *)true};
        HardwareSPI::master()->request(r);
    }

    /**
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(void *cbData) override {
        bitwisecopy((uint8_t *)sensor, NUMBITS, sizeof(*sensor), buffer, num);
    }
};
#endif //TANKSTM_H
