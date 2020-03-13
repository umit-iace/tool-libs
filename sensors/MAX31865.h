/** @file MAX31865.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef MAX31865_H
#define MAX31865_H

#include "stm/spi.h"

/**
 * TODO
 */
class MAX31865 : public ChipSelect {
public:
    enum Type {
        PT2WIRE = 0,
        PT3WIRE = 1,
        PT4WIRE = 0
    };

    MAX31865(uint32_t pin, GPIO_TypeDef *port, enum Type numWires, unsigned int Rref)
            : ChipSelect(pin, port), rRef(Rref) {
        this->setConfig(1 << 7 | // bias
                        1 << 6 | // auto conversion
                        numWires << 4 |
                        1 << 0 // 50Hz filter
                        );
    }

    double temp() {
        return dTemp;
    }

    void sense() {
        sensorData[0] = 0x1;
        HardwareSPI::master()->request(dataRequest);
        faultData[0] = 0x7;
        HardwareSPI::master()->request(faultRequest);

    }

    void callback() override {
        uint16_t rtd = (sensorData[1] << 8) | sensorData[2];
//            if (rtd & 1) { // fault
//                dTemp = 0.0001;
//            } else {
        dTemp = (double) (rtd >> 1) / (2 << 15) * rRef;
//            }
    }

private:
    double dTemp = 0;
    double rRef = 0;
    void setConfig(uint8_t val) {
        uint8_t data[2] = {0x80, val};
        SPIRequest r = {this, SPIRequest::MOSI, data, nullptr, 2, true};
        HardwareSPI::master()->request(r);
    }

    uint8_t faultData[2] = {0x7, 0};
    uint8_t sensorData[3] = {0x1, 0, 0};

    SPIRequest dataRequest = {this, SPIRequest::BOTH, sensorData, sensorData, 3, true};
    SPIRequest faultRequest = {this, SPIRequest::BOTH, faultData, faultData, 2, true};
};

#endif //MAX31865_H
