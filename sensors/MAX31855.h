/** @file MAX31855.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef MAX31855_H
#define MAX31855_H

#include <cmath>
#include <cstdint>

#include "stm/gpio.h"
#include "stm/spi.h"

class MAX31855 : ChipSelect {
    ReqeustQueue<SPIRequest> *bus = nullptr;
public:
    /**
     * Constructor
     * @param bus SPI request queue
     * @param cs Digital In/Out pin of chip select line
     */
    MAX31855(RequestQueue<SPIRequest> *bus, DIO cs) : bus(bus), ChipSelect(cs) { }

    /**
     * measured sensor temperature
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double temp() {
        return sTemp;
    }

    /**
     * measured internal chip temperature
     *
     * @return temperature in °C, NAN if sensor fault
     */
    double internal() {
        return iTemp;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        bus->request(new SPIRequest(
                this,
                SPIRequest::MISO,
                nullptr,
                buffer,
                sizeof(buffer),
                (void*)true)
        );
    }

    ///\cond false
    /**
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(void *cbData) override {
        flip(buffer, sizeof buffer);
        sensorData = (struct sensor &)buffer;
        if (!sensorData.FAULT) {
            sTemp = sensorData.TEMP * 0.25;
            iTemp =  sensorData.INTERNAL * 0.0625;
        } else {
            sTemp = NAN;
            iTemp = NAN;
        }
    }
    ///\endcond

private:
    ///\cond false
    struct sensor {
        uint8_t OC:1;                               ///< \b error: open circuit
        uint8_t SCG:1;                              ///< \b error: short to GND
        uint8_t SCV:1;                              ///< \b error: short to VCC
        uint8_t :1;
        int16_t INTERNAL:12;                       ///< internal measured temperature in 0.0625°C steps
        uint8_t FAULT:1;                            ///< \b error: fault in thermocouple reading
        uint8_t :1;
        int16_t TEMP:14;                           ///< thermocouple measured temperature in 0.25°C steps
    } sensorData = {};                              ///< sensor data struct. lsb to msb order.
    uint8_t buffer[4] = {};
    double sTemp = 0;
    double iTemp = 0;
    ///\endcond
};

#endif //MAX31855_H
