/** @file i2c.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_I2C_H
#define STM_I2C_H

#include "stm/gpio.h"
#include "stm/hal.h"
#include "utils/RequestQueue.h"

/**
 * struct defining an I2C request.
 * use this when requesting data from the \ref HardwareI2C
 */
class I2CRequest {
public:
    uint8_t address;     ///< 7bit address of device
    uint8_t memAddress;  ///< memory address
    uint8_t *pData;     ///< pointer to data
    uint32_t dataLen;   ///< number of bytes to transfer
    enum I2CMessageType {
        I2C_DIRECT_READ = 1 << 0,
        I2C_DIRECT_WRITE = 1 << 1,
        I2C_MEM_READ = 1 << 2,
        I2C_MEM_WRITE = 1 << 3,
    } type;

    void (*callback)(uint8_t *data);

    I2CRequest() = default;

    I2CRequest &operator=(const I2CRequest &other) {
        address = other.address;
        memAddress = other.memAddress;
        dataLen = other.dataLen;
        type = other.type;
        callback = other.callback;
        deepCopyDataPointer(other.pData);
        return *this;
    }

    I2CRequest(uint8_t address, uint8_t mem,
               uint8_t *pData, uint32_t dataLen,
               enum I2CMessageType type,
               void (*callback)(uint8_t *data)) :
            address(address), memAddress(mem),
            dataLen(dataLen), type(type),
            callback(callback) {
        deepCopyDataPointer(pData);
    }

    ~I2CRequest() {
        if (this->type & (I2C_DIRECT_WRITE | I2C_MEM_WRITE)) {
            delete[] this->pData;
        }
        address = 0;
        memAddress = 0;
        pData = nullptr;
        dataLen = 0;
        type = I2C_DIRECT_READ;
        callback = nullptr;
    }

private:
    void deepCopyDataPointer(uint8_t *other) {
        switch (type) {
            case I2C_DIRECT_READ:
            case I2C_MEM_READ:
                this->pData = other;
                break;
            case I2C_DIRECT_WRITE:
            case I2C_MEM_WRITE:
                this->pData = new uint8_t[dataLen]();
                for (unsigned int i = 0; i < dataLen; ++i) {
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

private:
    /**
     * override virtual RequestQueue function
     *
     * processes a request in the queue.
     * @param rq
     */
    void rqBegin(I2CRequest &rq) override {
        switch (rq.type) {
            case I2CRequest::I2C_DIRECT_READ:
                HAL_I2C_Master_Receive_IT(this->hI2C, rq.address << 1,
                                          rq.pData, rq.dataLen);
                break;
            case I2CRequest::I2C_DIRECT_WRITE:
                HAL_I2C_Master_Transmit_IT(this->hI2C, rq.address << 1,
                                           rq.pData, rq.dataLen);
                break;
            case I2CRequest::I2C_MEM_READ:
                HAL_I2C_Mem_Read_IT(this->hI2C, rq.address << 1,
                                    rq.memAddress, I2C_MEMADD_SIZE_8BIT,
                                    rq.pData, rq.dataLen);
                break;
            case I2CRequest::I2C_MEM_WRITE:
                HAL_I2C_Mem_Write_IT(this->hI2C, rq.address << 1,
                                     rq.memAddress, I2C_MEMADD_SIZE_8BIT,
                                     rq.pData, rq.dataLen);
                break;
        }
    }

    /**
     * timeout
     */
    void rqTimeout(I2CRequest &rq) override {
        HAL_I2C_Master_Abort_IT(this->hI2C, rq.address << 1);
        rqEnd();
    }

    /**
     * complete the data transfer, signal request completion
     * @param hi2c
     */
    static void transferComplete(I2C_HandleTypeDef *hi2c) {
        auto r = pThis->rqCurrent();
        if (r.callback) {
            r.callback(r.pData);
        }
        // signal request complete
        pThis->rqEnd();
    }

    /**
     * error callback.
     *
     * abort current request.
     */
    static void errorCallback(I2C_HandleTypeDef *hI2C) {
        pThis->rqEnd();
    }

    HardwareI2C(uint32_t iSdaPin, GPIO_TypeDef *iSdaPort, uint32_t iSdaAlternate,
                uint32_t iSclPin, GPIO_TypeDef *iSclPort, uint32_t iSclAlternate,
                I2C_TypeDef *i2c, uint32_t iBaud, uint32_t iAddr) : RequestQueue(50, HW_I2C_TIMEOUT) {
        AFIO(iSdaPin, iSdaPort, iSdaAlternate, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD);
        AFIO(iSclPin, iSclPort, iSclAlternate, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD);
        this->config(i2c, iBaud, iAddr);
    }

    void config(I2C_TypeDef *i2c, uint32_t iBaud, uint32_t iAddr) {
        *this->hI2C = {};
        this->hI2C->Instance = i2c;
#if defined(STM32F407xx)
        this->hI2C->Init.ClockSpeed = iBaud;
        this->hI2C->Init.DutyCycle = I2C_DUTYCYCLE_2;
#elif defined(STM32F767xx)
        // shoot me now
        // hardcoded to 400kHz if i2cclk 216MHz
        this->hI2C->Init.Timing = 0x7 << 28 | // presc
                            4 << 20 | // data setup time
                            4 << 16 | // data hold time
                            21 << 8 | // high period
                            40 << 0; // low period
#endif
        this->hI2C->Init.OwnAddress1 = iAddr;
        this->hI2C->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
        this->hI2C->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
        this->hI2C->Init.OwnAddress2 = 0;
        this->hI2C->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
        this->hI2C->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
        while (HAL_I2C_Init(this->hI2C) != HAL_OK);

        HAL_I2C_RegisterCallback(this->hI2C, HAL_I2C_MEM_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(this->hI2C, HAL_I2C_MEM_RX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(this->hI2C, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(this->hI2C, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, transferComplete);

        HAL_I2C_RegisterCallback(this->hI2C, HAL_I2C_ERROR_CB_ID, errorCallback);

        HAL_NVIC_SetPriority(iI2cErrorInterrupt, iI2cErrorPrePrio, iI2cErrorSubPrio);
        HAL_NVIC_EnableIRQ(iI2cErrorInterrupt);
        HAL_NVIC_SetPriority(iI2cEventInterrupt, iI2cEventPrePrio, iI2cEventSubPrio);
        HAL_NVIC_EnableIRQ(iI2cEventInterrupt);
    }

    //\cond false
    inline static HardwareI2C *pThis = nullptr;
    I2C_HandleTypeDef *hI2C;
    //\endcond
};

#endif //STM_I2C_H
