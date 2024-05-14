/** @file MAX31865.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once

#include <cmath>
#include <cstdint>

#include "sys/spi.h"

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
struct MAX31865 : SPI::Device {
    /// Sensor Type
    enum Type {
        PT2WIRE = 0, ///< 2 wire
        PT3WIRE = 1, ///< 3 wire
        PT4WIRE = 0, ///< 4 wire
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
    /// Sensor configuration
    struct Config {
        Type wires; ///< number of wires
        double Rnom; ///< nominal resistance
        double Rref; ///< reference resistor
    };

    /**
     * configure temperature sensor
     * @param bus SPI request queue
     * @param cs Digital In/Out pin of chip select line
     * @param c Hardware configuration of sensor
     *
     * configures sensor for auto conversion with 50Hz filter
     */
    MAX31865(Sink<SPI::Request> &bus,
            DIO cs,
            Config c = {
                .wires = PT2WIRE,
                .Rnom = 100,
                .Rref = 430,
                })
            : SPI::Device(bus, cs, {SPI::Mode::M3, SPI::FirstBit::MSB}), Rnom(c.Rnom), Rref(c.Rref) {
        setConfig(1 << 7 | // bias
                    1 << 6 | // auto conversion
                    c.wires << 4 |
                    1 << 1 | // clear faults
                    1 << 0 // 50Hz filter
                    );
        /* setLThr(0); */
        /* setHThr(0xffff); */
        getAllData();
    }

    /**
     * linear temperature approximation
     * @return temperature in °C, NAN if sensor fault
     */
    double tempLin() {
        if (data.rtd & 1) {
            clearFaults();
            return NAN;
        }
        return lin();
    }

    /**
     * quadratic temperature approximation
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double temp() {
        if (data.rtd & 1) {
            clearFaults();
            return NAN;
        }
        return nonlin();
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

    /*
     * callback when async read is finished
     */
    void callback(const SPI::Request rq) override {
        if (rq.dir == SPI::Request::MOSI) return;
        const uint8_t *raw = rq.data.buf;
        switch (rq.data.len) {
        case 2: // STATUS
            data.fault = raw[1];
            break;
        case 9: // All Data
            data.rtd = raw[2] << 8 | raw[3];
            data.threshold.high = raw[4]<<8 | raw[5];
            data.threshold.low  = raw[6]<<8 | raw[7];
            data.fault = raw[8];
            break;
        case 3: // resistance data
            data.rtd = raw[1] << 8 | raw[2];
            break;
        }
    }

    /**
     * @return fault byte read from sensor
     */
    uint8_t fault() {
        return data.fault;
    }

    /**
     * clear faults on sensor
     */
    void clearFaults() {
        data.rtd = data.rtd >> 1 << 1;
        getStatusData();
        setConfig(data.config | 1 << 1);
    }


    /**
     * set low fault threshold value
     * @param thr threshold
     */
    void setLThr(uint16_t thr) {
        bus.push({
                .dev = this,
                .data = {WRITE | REG_LOW_FAULT_THRESHOLD, (uint8_t)(thr>>8), (uint8_t)thr},
                .dir = SPI::Request::MOSI,
                });
    }

    /**
     * set high fault threshold value
     * @param thr threshold
     */
    void setHThr(uint16_t thr) {
        bus.push({
                .dev = this,
                .data = {WRITE | REG_HIGH_FAULT_THRESHOLD, (uint8_t)(thr>>8), (uint8_t)thr},
                .dir = SPI::Request::MOSI,
                });
    }

private:
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

    struct sensor {
        uint8_t config;
        uint8_t fault;
        struct threshold {
            uint16_t high, low;
        } threshold;
        int16_t rtd;
    } data;

    void setConfig(uint8_t val) {
        data.config = val;
        bus.push({
                .dev = this,
                .data = {WRITE|REG_CONFIG, data.config},
                .dir = SPI::Request::MOSI,
                });
    }

    void getTempData() {
        bus.push({
                .dev = this,
                .data = {READ|REG_RTD, 0, 0},
                .dir = SPI::Request::BOTH,
                });
    }

    void getStatusData() {
        bus.push({
                .dev = this,
                .data = {READ|REG_STATUS, 0},
                .dir = SPI::Request::BOTH,
                });
    }

    void getAllData() {
        bus.push({
                .dev = this,
                .data = {READ|REG_CONFIG, 0,0,0,0,0,0,0,0},
                .dir = SPI::Request::BOTH,
                });
    }
    double lin() {
        // resistance
        double Rrtd = (double) (data.rtd >> 1) / (1 << 15) * Rref;
        // linear: R(t) = R0 (1 + alpha*T)
        return (Rrtd - Rnom)/(Rnom * alpha);
    }
    double nonlin() {
        // resistance
        double Rrtd = (double) (data.rtd >> 1) / (1 << 15) * Rref;
        // quadratic:
        // from
        // R(t) = R0 (1 + aT + bT^2)
        // !! ONLY for temperatures > 0°C !!
        double dTemp = -a2b - sqrt(a2b*a2b - (Rnom - Rrtd)/(b*Rnom));
        /* double dTemp = (Z1 + sqrt(Z2 + Z3 * Rrtd)) / Z4; */
        return dTemp;
    }
};
