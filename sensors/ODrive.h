/** @file ODrive.h
 *
 * Copyright (c) 2021 IACE
 * ------------------------------------
 * Implements parts of the ODrive ASCII Protocol
 * Details here: https://docs.odriverobotics.com/ascii-protocol
 * The underlying UART Driver is referenced in the constructor
 * ------------------------------------
 */

#ifndef RIG_ODRIVE_H
#define RIG_ODRIVE_H

#include "stm/uart.h"
#include <cstdio>
#include <cstring>

class ODrive {
    
public:
    
    /**
     * Actuators
     */
    enum Actuator {
        M0 = 0,
        M1 = 1
    };
    
    /**
     * initialize the ODrive abstraction
     */
    ODrive(HardwareUART *uartInstance)
    {
        instance = uartInstance;
    }
    
    /**
     * Set motor speed by actuator number (M0 and M1) and the angular velocity in revolutions per minute.
     */
    void setSpeed(Actuator actuator, float revPerMin) {
        uint8_t UARTbuffer[64];
        sprintf((char*)UARTbuffer, "v %d %.2f\r\n", actuator, revPerMin);
        UARTRequest *urq = new UARTRequest(UARTbuffer, nullptr, strlen((char*)UARTbuffer), nullptr);
        instance->request(*urq);
    }
    
    /**
     * Get current motor speed by actuator number (M0 and M1) in revolutions per minute.
     */
    float getActualSpeed(Actuator actuator) {
        uint8_t UARTbuffer[64];
        sprintf((char*)UARTbuffer, "f %d\r\n", actuator);
        UARTRequest *urq = new UARTRequest(UARTbuffer, nullptr, strlen((char*)UARTbuffer), nullptr);
        instance->request(*urq);
    }
    
private:
    
    HardwareUART *instance = nullptr;
    
    
};


#endif //RIG_ODRIVE_H
