/** @file i2c.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_I2C_H
#define STM_I2C_H

#include "stm/gpio.h"
#include "stm/hal.h"
#include "utils/RequestQueue.h"

extern "C" {
I2C_HandleTypeDef hHWI2C;
};

/**
 * struct defining an I2C request.
 * use this when requesting data from the \ref HardwareI2C
 */
class I2CRequest {
public:
    uint8_t address;     ///< 7bit address of device
    enum eDir {
        WRITE,
        READ
    } dir;              ///< direction the data should travel
    uint8_t memAddress;  ///< memory address
    uint8_t *pData;     ///< pointer to data
    uint32_t dataLen;   ///< number of bytes to transfer
    void (*process)(I2CRequest &, I2C_HandleTypeDef *);
    void (*callback)(uint8_t *data);

    I2CRequest() = default;

    I2CRequest &operator=(const I2CRequest &other) {
        address = other.address;
        dir = other.dir;
        memAddress = other.memAddress;
        dataLen = other.dataLen;
        process = other.process;
        callback = other.callback;
        deepCopyDataPointer(other.pData);
        return *this;
    }

    I2CRequest(uint8_t address, enum eDir dir, uint8_t mem,
                uint8_t *pData, uint32_t dataLen,
                void (*process)(I2CRequest &rq, I2C_HandleTypeDef *hi2c),
                void (*callback)(uint8_t *data)) :
            address(address), dir(dir), memAddress(mem),
            process(process),
            callback(callback), dataLen(dataLen) {
        deepCopyDataPointer(pData);
    }

    ~I2CRequest() {
        if (this->dir == WRITE) {
            delete[] this->pData;
        }
        address = 0;
        dir = WRITE;
        memAddress = 0;
        pData = nullptr;
        dataLen = 0;
        process = nullptr;
        callback = nullptr;
    }

private:
    void deepCopyDataPointer(uint8_t* other) {
        switch (dir) {
            case READ:
                this->pData = other;
                break;
            case WRITE:
                this->pData = new uint8_t[dataLen]();
                for (int i = 0; i < dataLen; ++i) {
                    *(this->pData + i) = *other++;
                }
                break;
        }
    }
};

/**
 * @brief Template class for hardware based I2C derivations
 */
class HardwareI2C : public RequestQueue<I2CRequest> {
public:
    /**
     * override virtual RequestQueue function
     * @return current time in ms
     */
    unsigned long getTime() override {
        return HAL_GetTick();
    }

    static HardwareI2C *master() {
        if (!pThis) {
            pThis = new HardwareI2C();
        }
        return pThis;
    }

    /**
     * override virtual RequestQueue function
     *
     * processes a request in the queue.
     * @param rq
     */
    void processRequest(I2CRequest &rq) override {
        rq.process(rq, &hI2C);
    }

private:
    /**
     * complete the data transfer, signal request completion
     * @param hi2c
     */
    static void transferComplete(I2C_HandleTypeDef *hi2c) {
        auto r = pThis->lastRequest();
        if (r.callback) {
            r.callback(r.pData);
        }
        // signal request complete
        pThis->endProcess();
    }

    HardwareI2C() : RequestQueue(50, HW_I2C_TIMEOUT)  {
        AFIO(HW_I2C_SDA_PIN, HW_I2C_SDA_PORT, HW_I2C_SDA_ALTERNATE,
            GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD);
        AFIO(HW_I2C_SCL_PIN, HW_I2C_SCL_PORT, HW_I2C_SCL_ALTERNATE,
            GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD);
        this->config(HW_I2C, HW_I2C_BAUD);
    }

    void config(I2C_TypeDef *i2c, uint32_t baud) {
        hI2C.Instance = i2c;
#if defined(STM32F407xx)
        hI2C.Init.ClockSpeed = baud;
        hI2C.Init.DutyCycle = I2C_DUTYCYCLE_2;
#elif defined(STM32F767xx)
        // shoot me now
        // hardcoded to 400kHz if i2cclk 216MHz
        hI2C.Init.Timing = 0x7 << 28 | // presc
                            4 << 20 | // data setup time
                            4 << 16 | // data hold time
                            21 << 8 | // high period
                            40 << 0; // low period
#endif
        hI2C.Init.OwnAddress1 = 0;
        hI2C.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
        hI2C.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
        hI2C.Init.OwnAddress2 = 0;
        hI2C.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
        hI2C.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
        while (HAL_I2C_Init(&hI2C) != HAL_OK);

        HAL_I2C_RegisterCallback(&hI2C, HAL_I2C_MEM_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&hI2C, HAL_I2C_MEM_RX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&hI2C, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&hI2C, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, transferComplete);

        HAL_NVIC_SetPriority(I2C1_ER_IRQn, HW_I2C_IT_PRIO);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
        HAL_NVIC_SetPriority(I2C1_EV_IRQn, HW_I2C_IT_PRIO);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    }

    //\cond false
    inline static HardwareI2C *pThis = nullptr;
    I2C_HandleTypeDef &hI2C = hHWI2C;
    //\endcond
};

extern "C" void I2C1_EV_IRQHandler() {
    HAL_I2C_EV_IRQHandler(&hHWI2C);
}

extern "C" void I2C1_ER_IRQHandler() {
    HAL_I2C_ER_IRQHandler(&hHWI2C);
}
#endif //STM_I2C_H
