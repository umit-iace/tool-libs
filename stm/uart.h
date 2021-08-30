//
// Created by UMIT IACE on 26.08.2021.
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

    void (*callback)(uint8_t *data);

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
    UARTRequest(uint8_t *tData, uint8_t *rData, uint32_t dataLen, void *cbData, void (*callback)(uint8_t *data) = nullptr) :
    tData(tData), rData(rData), dataLen(dataLen), cbData(cbData) {
        this->callback = callback;
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
        HAL_UART_Transmit_DMA(this->hUart, rq.tData, rq.dataLen);
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
    * complete the data transfer, signal request completion
    * @param hi2c
    */
    static void transferComplete(UART_HandleTypeDef *huart) {
        auto r = pThis->rqCurrent();
        if (r.callback) {
            r.callback(r.rData);
        }
        // signal request complete
        pThis->rqEnd();
    }

    /**
     * error callback.
     *
     * abort current request.
     */
    static void errorCallback(UART_HandleTypeDef *hI2C) {
        pThis->rqEnd();
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
     * @param DMAHandle dma handle, set to nullptr when dma is not used
     * @param DMAChannel dma channel
     */HardwareUART(uint32_t iRXPin, GPIO_TypeDef *gpioRXPort, uint8_t iRXAlternate,
                 uint32_t iTXPin, GPIO_TypeDef *gpioTXPort, uint8_t iTXAlternate,
                 USART_TypeDef *dUsart, uint32_t iBaudRate, UART_HandleTypeDef *hUart,
                 DMA_Stream_TypeDef *DMAHandle, uint32_t DMAChannel) :
                 RequestQueue(50, 100), hUart(hUart), dUsart(dUsart), iBaudRate(iBaudRate)  {
        AFIO(iRXPin, gpioRXPort, iRXAlternate, GPIO_NOPULL);
        AFIO(iTXPin, gpioTXPort, iTXAlternate, GPIO_NOPULL);

        this->DMAChannel = DMAChannel;
        this->DMAHandle = DMAHandle;
        this->config();
        pThis = this;
    }


    //\cond false
    inline static HardwareUART *pThis = nullptr;
    UART_HandleTypeDef *hUart;
    //\endcond

private:
    //\cond false
    USART_TypeDef *dUsart;
    uint32_t iBaudRate;
    DMA_Stream_TypeDef *DMAHandle;
    uint32_t DMAChannel;
    DMA_HandleTypeDef hdma_usart_tx;



    void DMA1_Stream6_IRQHandler(void)
    {
        HAL_DMA_IRQHandler(&hdma_usart_tx);
    }

    void USART2_IRQHandler(void)
    {
        HAL_UART_IRQHandler(this->hUart);
    }

    void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
    {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_3);
    }

    void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart)
    {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_3);
    }

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

        HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);

        if (DMAHandle != nullptr)
        {
            hdma_usart_tx.Instance = DMAHandle;
            hdma_usart_tx.Init.Channel = DMAChannel;
            hdma_usart_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
            hdma_usart_tx.Init.PeriphInc = DMA_PINC_DISABLE;
            hdma_usart_tx.Init.MemInc = DMA_MINC_ENABLE;
            hdma_usart_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
            hdma_usart_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
            hdma_usart_tx.Init.Mode = DMA_NORMAL;
            hdma_usart_tx.Init.Priority = DMA_PRIORITY_LOW;
            hdma_usart_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
            while(HAL_DMA_Init(&hdma_usart_tx) != HAL_OK);
            __HAL_LINKDMA(this->hUart,hdmatx,hdma_usart_tx);
            // HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
            // HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
        }

        // HAL_UART_RegisterCallback(this->hUart, HAL_UART_TX_COMPLETE_CB_ID, transferComplete);
        // HAL_UART_RegisterCallback(this->hUart, HAL_UART_TX_HALFCOMPLETE_CB_ID, transferComplete);
        // HAL_UART_RegisterCallback(this->hUart, HAL_UART_RX_COMPLETE_CB_ID, transferComplete);
        // HAL_UART_RegisterCallback(this->hUart, HAL_UART_ERROR_CB_ID, errorCallback);




    };
    //\endcond
};

#endif //RIG_UART_H
