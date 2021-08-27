/** @file ODrive.h
 *
 * Copyright (c) 2021 IACE
 */

#ifndef RIG_ODRIVE_H
#define RIG_ODRIVE_H

#include "stm/uart.h"

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
    
private:
    
    HardwareUART *instance = nullptr;
    
    
};


#endif //RIG_ODRIVE_H
