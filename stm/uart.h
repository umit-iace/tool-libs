//
// Created by florian on 03.08.2021.
//

#ifndef RIG_UART_H
#define RIG_UART_H



#include <cstdint>

#include "stm/gpio.h"
#include "stm/hal.h"
#include "utils/RequestQueue.h"

/**
 * struct defining a UART request.
 * use this when requesting data from the \ref HardwareUART
 */
class UARTRequest {
public:

    uint8_t *tData;     ///< pointer to data to be transmitted
    uint8_t *rData;     ///< pointer to receive buffer
    uint32_t dataLen;   ///< number of bytes to transfer
    void *cbData;     ///< data to use in callback

    /**
     * Standard constructor
     */
    UARTRequest() {};

    /**
     * @param dir
     * @param tData
     * @param rData
     * @param dataLen
     * @param cbData
     */
    UARTRequest(uint8_t *tData, uint8_t *rData, uint32_t dataLen, void *cbData) :
    tData(tData), rData(rData), dataLen(dataLen), cbData(cbData) {
    }

    ~UARTRequest() {
        tData = nullptr;
        rData = nullptr;
        dataLen = 0;
        cbData = nullptr;
    }

    UARTRequest &operator=(const UARTRequest &other) {
        tData = other.tData;
        rData = other.rData;
        dataLen = other.dataLen;
        cbData = other.cbData;
        return *this;
    }
};


/**
 * @brief Template class for hardware based UART derivations
 */
class HardwareUART : public RequestQueue<UARTRequest> {
public:
    /**
         * override virtual RequestQueue function
         * @return current time in ms
         */
    unsigned long getTime() override {
        return HAL_GetTick();
    }

    /**
     * override virtual RequestQueue function
     *
     * processes a request in the queue.
     * @param rq
     */
    void rqBegin(UARTRequest &rq) override {
        // transfer the data
        HAL_UART_Transmit(this->hUart, rq.tData, rq.dataLen, 100);
        rqEnd();
    }

    /**
     * override virtual RequestQueue function
     *
     * timeout -> abort
     */
    void rqTimeout(UARTRequest &rq) override {
        HAL_UART_Abort(this->hUart);
        rqEnd();
    }

    /**
     * Constructor, that initialize the RX and TX pins and configure UART instance
     * @param iRXPin RX pin address
     * @param gpioRXPort definition of RX port
     * @param iRXAlternate address of alternate RX pin functionality
     * @param iTXPin TX pin address
     * @param gpioTXPort definition of TX port
     * @param iTXAlternate address of alternate TX pin functionality
     * @param dUsart definition of used UART
     * @param iBaudRate baud rate
     * @param hUart uart handle
     */
    HardwareUART(uint32_t iRXPin, GPIO_TypeDef *gpioRXPort, uint8_t iRXAlternate,
                 uint32_t iTXPin, GPIO_TypeDef *gpioTXPort, uint8_t iTXAlternate,
                 USART_TypeDef *dUsart, uint32_t iBaudRate, UART_HandleTypeDef *hUart) :
                 RequestQueue(50, 100), hUart(hUart), dUsart(dUsart), iBaudRate(iBaudRate) {
        AFIO(iRXPin, gpioRXPort, iRXAlternate, GPIO_PULLUP);
        AFIO(iTXPin, gpioTXPort, iTXAlternate, GPIO_PULLUP);

        this->config();
    }

    //\cond false
    UART_HandleTypeDef *hUart;
    //\endcond

private:
    //\cond false
    USART_TypeDef *dUsart;
    uint32_t iBaudRate;

    void config() {
        *this->hUart = {};
        this->hUart->Instance = this->dUsart;
        this->hUart->Init.BaudRate = this->iBaudRate;
        this->hUart->Init.WordLength = UART_WORDLENGTH_8B;
        this->hUart->Init.StopBits = UART_STOPBITS_1;
        this->hUart->Init.Parity = UART_PARITY_NONE;
        this->hUart->Init.Mode = UART_MODE_TX_RX;
        this->hUart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
        this->hUart->Init.OverSampling = UART_OVERSAMPLING_16;
        this->hUart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
        this->hUart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
        while (HAL_UART_Init(this->hUart) != HAL_OK);
    };
    //\endcond
};

#endif //RIG_UART_H
