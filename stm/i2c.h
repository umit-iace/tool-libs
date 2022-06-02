/** @file i2c.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_I2C_H
#define STM_I2C_H

#include "stm/hal.h"
#include "utils/RequestQueue.h"

/**
 * generic I2C device wrapper.
 *
 * Implement I2C devices with this as a base class to make use of the
 * asynchronous HardwareI2C bus.
 *
 * @note currently only implements 7bit addressing
 */
class I2CDevice {
public:
    /// unshifted 7bit I2C device address
    const uint8_t addr;

    /**
     * @param addr unshifted I2C device address
     */
    I2CDevice(uint8_t addr) : addr(addr) { }

    /**
     * callback. is called as soon as requested data arrived over wire.
     *
     * @param cbData this can be used to identify the request causing
     * the callback. This can be anything, pointer is _not_ followed.
     */
    virtual void callback(void *cbData) { }
};

/**
 * struct defining an I2C request.
 * use this when requesting data from the \ref HardwareI2C
 */
class I2CRequest {
public:
    I2CDevice *dev;     ///< pointer to the calling \ref I2CDevice instance
    uint8_t memAddress;  ///< memory address
    uint8_t *pData;     ///< pointer to data
    uint32_t dataLen;   ///< number of bytes to transfer
    /// type of message
    enum I2CMessageType {
        I2C_DIRECT_READ = 1 << 0,
        I2C_DIRECT_WRITE = 1 << 1,
        I2C_MEM_READ = 1 << 2,
        I2C_MEM_WRITE = 1 << 3,
    } type;
    void *cbData;       ///< data to use in callback. pointer is _not_ followed

    /**
     *
     * @param dev pointer to the calling \ref I2CDevice instance
     * @param mem memory address. only used for `I2C_MEM_*` type requests
     * @param pData pointer to data
     * @param dataLen number of bytes to transfer
     * @param type type of message
     * @param cbData data to use in callback. pointer is _not_ followed
     */
    I2CRequest(I2CDevice *dev, uint8_t mem,
                uint8_t *pData, uint32_t dataLen,
                enum I2CMessageType type,
                void *cbData) :
            dev(dev), memAddress(mem),
            dataLen(dataLen), type(type),
            cbData(cbData) {
        deepCopyDataPointer(pData);
    }

    ~I2CRequest() {
        if (this->type & (I2C_DIRECT_WRITE | I2C_MEM_WRITE)) {
            delete[] this->pData;
        }
        dev = 0;
        memAddress = 0;
        pData = nullptr;
        dataLen = 0;
        type = I2C_DIRECT_READ;
        cbData = nullptr;
    }

private:
    void deepCopyDataPointer(uint8_t* other) {
        switch (this->type) {
            case I2C_DIRECT_READ:
            case I2C_MEM_READ:
                this->pData = other;
                break;
            case I2C_DIRECT_WRITE:
            case I2C_MEM_WRITE:
                this->pData = new uint8_t[dataLen]();
                for (unsigned int i = 0; i < dataLen; ++i) {
                    this->pData[i] = other[i];
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
    void rqBegin(I2CRequest *rq) override {
        switch (rq->type) {
            case I2CRequest::I2C_DIRECT_READ:
                HAL_I2C_Master_Receive_IT(&handle, rq->dev->addr << 1,
                                          rq->pData, rq->dataLen);
                break;
            case I2CRequest::I2C_DIRECT_WRITE:
                HAL_I2C_Master_Transmit_IT(&handle, rq->dev->addr << 1,
                                           rq->pData, rq->dataLen);
                break;
            case I2CRequest::I2C_MEM_READ:
                HAL_I2C_Mem_Read_IT(&handle, rq->dev->addr << 1,
                                    rq->memAddress, I2C_MEMADD_SIZE_8BIT,
                                    rq->pData, rq->dataLen);
                break;
            case I2CRequest::I2C_MEM_WRITE:
                HAL_I2C_Mem_Write_IT(&handle, rq->dev->addr << 1,
                                     rq->memAddress, I2C_MEMADD_SIZE_8BIT,
                                     rq->pData, rq->dataLen);
                break;
        }
    }

    /**
     * timeout
     */
    void rqTimeout(I2CRequest *rq) override {
        HAL_I2C_Master_Abort_IT(&handle, rq->dev->addr << 1);
        rqEnd();
    }

    /**
     * complete the data transfer, signal request completion
     * @param hi2c
     */
    static void transferComplete(I2C_HandleTypeDef *hi2c) {
        auto r = pThis->rqCurrent();
        if (r->cbData) {
            r->dev->callback(r->cbData);
        }
        // signal request completion
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
    //\cond false
    inline static HardwareI2C *pThis = nullptr;
    //\endcond

public:
    struct Conf {
        I2C_TypeDef *i2c;
        AFIO sda, scl;
        uint32_t baud;
        uint32_t timeout;
    };
    HardwareI2C(Conf conf) :
            RequestQueue(50, conf.timeout)
    {
        handle.Instance = conf.i2c;
        handle.Init = {
#if defined(STM32F4)
            .ClockSpeed = conf.baud,
            .DutyCycle = I2C_DUTYCYCLE_2,
#elif defined(STM32F7)
            // shoot me now
            // hardcoded to 400kHz if i2cclk 216MHz
            .Timing = 0x7 << 28 | // presc
                            4 << 20 | // data setup time
                            4 << 16 | // data hold time
                            21 << 8 | // high period
                            40 << 0, // low period
#endif
            .OwnAddress1 = 0,
            .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
            .DualAddressMode = I2C_DUALADDRESS_DISABLE,
            .OwnAddress2 = 0,
            .GeneralCallMode = I2C_GENERALCALL_DISABLE,
            .NoStretchMode = I2C_NOSTRETCH_DISABLE,
        };
        while (HAL_I2C_Init(&handle) != HAL_OK);

        HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&handle, HAL_I2C_MEM_RX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, transferComplete);
        HAL_I2C_RegisterCallback(&handle, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, transferComplete);

        HAL_I2C_RegisterCallback(&handle, HAL_I2C_ERROR_CB_ID, errorCallback);

        pThis = this;
    }

    I2C_HandleTypeDef handle{};
};
#endif //STM_I2C_H
