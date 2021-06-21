/** @file MAX31865.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef MAX31865_H
#define MAX31865_H

#include <cmath>
#include <cstdint>

#include "stm/hal.h"
#include "stm/spi.h"

/**
 * Implementation of MAX31865 based temperature sensor.
 *
 * supports 2 to 4 wire sensors of arbitrary nominal resistances,
 * and arbitrary reference resistors.
 *
 * @warning reference resistor must be greater than sensor resistor
 * (for platinum based resistors Rref = 4 * Rnom works best)
 *
 * @warning implemented only for temperatures > 0 degrees C.
 * see
 * https://analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf
 * for details
 */
class MAX31865 : ChipSelect {
public:
    /// Sensor Type
    enum Type {
        PT2WIRE = 0,
        PT3WIRE = 1,
        PT4WIRE = 0
    };

    ///\cond false
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
    ///\endcond

    /**
     * configure temperature sensor
     * @param pin chip select pin number
     * @param port chip select pin port
     * @param numWires sensor type enum
     * @param Rref reference resistance
     * @param Rnom nominal resistance of sensor at 0°C
     *
     * configures sensor for auto conversion with 50Hz filter
     */
    MAX31865(uint32_t pin, GPIO_TypeDef *port, enum Type numWires, double Rref, double Rnom)
            : ChipSelect(pin, port), Rnom(Rnom), Rref(Rref) {
        this->setConfig(1 << 7 | // bias
                        1 << 6 | // auto conversion
                        numWires << 4 |
                        1 << 1 | // clear faults
                        1 << 0 // 50Hz filter
        );
        this->setLThr(0);
        this->setHThr(0xffff);
    }

    /**
     * linear temperature approximation
     * @return temperature in °C, NAN if sensor fault
     */
    double tempLin() {
        return dTempLin;
    }

    /**
     * quadratic temperature approximation
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double temp() {
        return dTemp;
    }

    /**
     * async read temperature data from sensor
     */
    void sense() {
        getTempData();
    }

    /**
     * async read fault data from sensor
     */
    void readFault() {
        getStatusData();
    }

    /**
     * async read all data from sensor
     */
    void readAllData() {
        getAllData();
    }

    ///\cond false
    /**
     * callback when async read is finished
     */
    void callback(void *userData) override {
        auto reqType = (enum callbackUserData &)userData;
        uint16_t rtd;
        switch (reqType) {
            case ALLDATAREQ:
                sensorData[1] = allData[2];
                sensorData[2] = allData[3];
                hThr = allData[4]<<8 | allData[5];
                lThr = allData[6]<<8 | allData[7];
                statusData[1] = allData[8];
                iFault = statusData[1];
                /* fallthrough */
            case TEMPREQ:
                rtd = (sensorData[1] << 8) | sensorData[2];
                if (rtd & 1) { // fault
                    dTemp = NAN;
                    dTempLin = NAN;
                } else {
                    // resistance
                    double Rrtd = (double) (rtd >> 1) / (1 << 15) * Rref;
                    // linear: R(t) = R0 (1 + alpha*T)
                    dTempLin = (Rrtd - Rnom)/(Rnom * alpha);
                    // quadratic:
                    // from
                    // R(t) = R0 (1 + aT + bT^2)
                    // !! ONLY for temperatures > 0°C !!
                    dTemp = -a2b - sqrt(a2b*a2b - (Rnom - Rrtd)/(b*Rnom));
                    /* dTemp = (Z1 + sqrt(Z2 + Z3 * Rrtd)) / Z4; */
                }
                break;
            case STATUSREQ:
                iFault = statusData[1];
                break;
            case CONFIGREQ:
                // maybe do something
                break;
            case NOREQ:
                // intentionally empty
                break;
        }
    }
    ///\endcond

    /**
     * @return fault byte read from sensor
     */
    uint8_t fault() {
        return this->iFault;
    }

    /**
     * clear faults on sensor
     */
    void clearFaults() {
        config |= 1 << 1;
        setConfig(config);
    }

    /**
     * set low fault threshold value
     * @param thr threshold
     */
    void setLThr(uint16_t thr) {
        static uint8_t data[3] = {WRITE | REG_LOW_FAULT_THRESHOLD,
                    (uint8_t)(thr >> 8), (uint8_t)thr};

        HardwareSPI::master()->request(new SPIRequest(this, SPIRequest::MOSI, data, nullptr,
                    3, (void*)NOREQ));
    }

    /**
     * set high fault threshold value
     * @param thr threshold
     */
    void setHThr(uint16_t thr) {
        static uint8_t data[3] = {WRITE | REG_HIGH_FAULT_THRESHOLD,
                    (uint8_t)(thr >> 8), (uint8_t)thr};

        HardwareSPI::master()->request(new SPIRequest(this, SPIRequest::MOSI, data, nullptr,
                    3, (void*)NOREQ));
    }

private:
    ///\cond false
    const double Rnom = 1000;
    const double Rref = 0;
    // from MAX31865 datasheet
    const double alpha = 0.00385055;
    const double a = 3.9083e-3;
    const double b = -5.775e-7;
    const double a2b = a/(2*b);
    // from application note
    /* const double Z1 = -3.9083e-3; */
    /* const double Z2 = 17.58480889e-6; */
    /* const double Z3 = 4 * b / Rnom; */
    /* const double Z4 = 2*b; */

    uint8_t config = 0;
    double dTempLin = 0;
    double dTemp = 0;
    uint8_t iFault = 0;
    uint16_t hThr = 0;
    uint16_t lThr = 0;

    uint8_t sensorData[3] = {};
    uint8_t statusData[2] = {};
    uint8_t allData[9] = {};

    enum callbackUserData {
        NOREQ,
        ALLDATAREQ,
        TEMPREQ,
        STATUSREQ,
        CONFIGREQ
    };

    void setConfig(uint8_t val) {
        this->config = val;
        uint8_t data[2] = {WRITE | REG_CONFIG, config};
        HardwareSPI::master()->request(new SPIRequest(
                this,
                SPIRequest::MOSI,
                data,
                nullptr,
                2,
                (void *) CONFIGREQ)
        );
    }

    void getTempData() {
        uint8_t tx[3] = {READ | REG_RTD, 0, 0};
        HardwareSPI::master()->request(new SPIRequest(
                this,
                SPIRequest::BOTH,
                tx,
                sensorData,
                sizeof tx,
                (void *) TEMPREQ)
        );
    }

    void getStatusData() {
        uint8_t tx[2] = {READ | REG_STATUS, 0};
        HardwareSPI::master()->request(new SPIRequest(
                this,
                SPIRequest::BOTH,
                tx,
                statusData,
                sizeof tx,
                (void *) STATUSREQ)
        );
    }

    void getAllData() {
        allData[0] = READ | REG_CONFIG;
        HardwareSPI::master()->request(new SPIRequest(
                this,
                SPIRequest::BOTH,
                allData,
                allData,
                9,
                (void *) ALLDATAREQ)
        );
    }
    ///\endcond
};

#endif //MAX31865_H
