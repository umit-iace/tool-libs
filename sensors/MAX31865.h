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
    // Sensor Type
    enum Type {
        PT2WIRE = 0,
        PT3WIRE = 1,
        PT4WIRE = 0
    };

    // Register Addresses
    enum {
        REG_CONFIG = 0,
        REG_RTD = 1,
        REG_HIGH_FAULT_THRESHOLD = 3,
        REG_LOW_FAULT_THRESHOLD = 5,
        REG_STATUS = 7
    };

    // Operation on register
    enum {
        READ = 0,
        WRITE = 0x80
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
        getTempData();
        getStatusData();
    }

    void callback(uint8_t userData) override {
        enum callbackUserData reqType = (enum callbackUserData)userData;
        switch (reqType) {
            case TEMPREQ: {
                uint16_t rtd = (sensorData[1] << 8) | sensorData[2];
                if (!(rtd & 1)) { // no fault
                    dTemp = (double) (rtd >> 1) / (2 << 15) * rRef;
                }
                break;
                }
            case STATUSREQ:
                iFault = statusData[1];
                break;
            case CONFIGREQ:
                // maybe do something
                break;
        }
    }

    void clearFaults() {
        config |= 1 << 1;
        setConfig(config);
    }

private:
    uint8_t config = 0;
    double dTemp = 0;
    double rRef = 0;
    uint8_t iFault = 0;

    uint8_t sensorData[3] = {};
    uint8_t statusData[2] = {};

    enum callbackUserData {
        TEMPREQ,
        STATUSREQ,
        CONFIGREQ
    };

    void setConfig(uint8_t val) {
        config = val;
        uint8_t data[2] = {WRITE | REG_CONFIG, config};
        SPIRequest r = {this, SPIRequest::MOSI, data, nullptr, 2, CONFIGREQ};
        HardwareSPI::master()->request(r);
    }

    void getTempData() {
        uint8_t tx[3] = {READ | REG_RTD, 0, 0};
        SPIRequest r = {this, SPIRequest::BOTH, tx, sensorData, sizeof tx, TEMPREQ};
        HardwareSPI::master()->request(r);
    }

    void getStatusData() {
        uint8_t tx[2] = {READ | REG_STATUS, 0};
        SPIRequest r = {this, SPIRequest::BOTH, tx, statusData, sizeof tx, STATUSREQ};
        HardwareSPI::master()->request(r);
    }

};

#endif //MAX31865_H
