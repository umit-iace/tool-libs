/** @file MAX31855.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef MAX31855_H
#define MAX31855_H

#include "stm/spi.h"
#include "math.h"

class MAX31855 : public ChipSelect {
public:
    /**
     * Constructor
     * @param pin GPIO pin of chip select line
     * @param port GPIO port of chip select line
     */
    MAX31855(uint32_t pin, GPIO_TypeDef *port) : ChipSelect(pin, port) { }

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
        SPIRequest r = {this, SPIRequest::MISO, nullptr, buffer, sizeof(buffer), (void*)true};
        HardwareSPI::master()->request(r);
    }

    ///\cond false
    /**
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(void *cbData) override {
        bitwisecopy((uint8_t *)&sensorData, 32, sizeof(sensorData), buffer, 1);
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
    struct __packed {
        uint8_t OC:1;                               ///< \b error: open circuit
        uint8_t SCG:1;                              ///< \b error: short to GND
        uint8_t SCV:1;                              ///< \b error: short to VCC
        uint8_t :1;
        uint16_t INTERNAL:12;                       ///< internal measured temperature in 0.0625°C steps
        uint8_t FAULT:1;                            ///< \b error: fault in thermocouple reading
        uint8_t :1;
        uint16_t TEMP:14;                           ///< thermocouple measured temperature in 0.25°C steps
    } sensorData = {};                              ///< sensor data struct. lsb to msb order.
    uint8_t buffer[4] = {};
    double sTemp = 0;
    double iTemp = 0;
    ///\endcond
};

#endif //MAX31855_H
