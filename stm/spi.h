/** @file spi.h
 *
 * Copyright (c) 2019 IACE
 *
 **********************************************
 * When using this, use the following line in your interrupt header
 * file (e.g. `stm32f7xx_it.h`):
 *
 *      extern SPI_HandleTypeDef hHWSPI;
 *
 * and put the following interrupt handler in your interrupt implementation
 * file (e.g. `stm32f7xx_it.c`)
 *
 * (**attention**: configure the correct SPI peripheral according to your
 * defined HW_SPI):
 *
 *      void SPI3_IRQHandler(void) {
 *          HAL_SPI_IRQHandler(&hHWSPI);
 *      }
 *
 **********************************************
 */

#ifndef STM_SPI_H
#define STM_SPI_H

#include <cstdint>

#include "stm/gpio.h"
#include "stm/hal.h"
#include "utils/RequestQueue.h"

class HardwareSPI;

/**
 * Class abstracting an SPI device.
 * Implement your SPI device with this as a base class, then you can use the
 * HardwareSPI to send/get data over the wire.
 */
class ChipSelect {
protected:
    //\cond false
    DIO pin;
    HardwareSPI *hwSPI;
    //\endcond
public:
    /**
     * @param iCSPIN chip select pin number
     * @param gpioCSPort chip select pin port
     */
    ChipSelect(uint32_t iCSPIN, GPIO_TypeDef *gpioCSPort,
               HardwareSPI *hwSPI) :
            pin(iCSPIN, gpioCSPort), hwSPI(hwSPI) {
        this->selectChip(false);
    }

    /**
     * activate chip
     * @param sel true/false
     */
    void selectChip(bool sel) {
        this->pin.set(!sel);
    }

    /**
     * callback. is called as soon as requested data arrived over wire.
     */
    virtual void callback(void *cbData) {}
};

/**
 * struct defining an SPI request.
 * use this when requesting data from the \ref HardwareSPI
 */
class SPIRequest {
public:
    ChipSelect *cs;     ///< pointer the \ref ChipSelect instance requesting data
    /// direction the data should travel
    enum eDir {
        MOSI,
        MISO,
        BOTH
    } dir;
    uint8_t *tData;     ///< pointer to data to be transmitted
    uint8_t *rData;     ///< pointer to receive buffer
    uint32_t dataLen;   ///< number of bytes to transfer
    void *cbData;     ///< data to use in callback

    /**
     * Standard constructor
     */
    SPIRequest() {};

    /**
     * @param cs
     * @param dir
     * @param tData
     * @param rData
     * @param dataLen
     * @param cbData
     */
    SPIRequest(ChipSelect *cs, enum eDir dir, uint8_t *tData, uint8_t *rData,
               uint32_t dataLen, void *cbData) :
            cs(cs), dir(dir), tData(tData), rData(rData),
            dataLen(dataLen), cbData(cbData) {
    }

    ~SPIRequest() {
        tData = nullptr;
        cs = nullptr;
        dir = MOSI;
        tData = nullptr;
        rData = nullptr;
        dataLen = 0;
        cbData = nullptr;
    }

    SPIRequest &operator=(const SPIRequest &other) {
        cs = other.cs;
        dir = other.dir;
        tData = other.tData;
        rData = other.rData;
        dataLen = other.dataLen;
        cbData = other.cbData;
        return *this;
    }
};

/**
 * @brief Template class for hardware based SPI derivations
 */
class HardwareSPI : public RequestQueue<SPIRequest> {
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
    void rqBegin(SPIRequest &rq) override {
        // activate the chip
        rq.cs->selectChip(true);

        // transfer the data
        switch (rq.dir) {
            case SPIRequest::MOSI:
                HAL_SPI_Transmit_IT(this->hSPI, rq.tData, rq.dataLen);
                break;
            case SPIRequest::MISO:
                HAL_SPI_Receive_IT(this->hSPI, rq.rData, rq.dataLen);
                break;
            case SPIRequest::BOTH:
                HAL_SPI_TransmitReceive_IT(this->hSPI, rq.tData, rq.rData, rq.dataLen);
                break;
        }
    }

    /**
     * override virtual RequestQueue function
     *
     * timeout -> abort
     */
    void rqTimeout(SPIRequest &rq) override {
        HAL_SPI_Abort_IT(this->hSPI);
        rq.cs->selectChip(false);
        rqEnd();
    }

private:
    /**
     * complete the data transfer, signal request completion
     * @param hspi
     */
    static void transferComplete(SPI_HandleTypeDef *hspi) {
        auto &r = pThis->rqCurrent();
        // deactivate chip
        r.cs->selectChip(false);
        // signal data arrival
        if (r.cbData) {
            r.cs->callback(r.cbData);
        }
        // signal request complete
        pThis->rqEnd();
    }

public:
    /**
     * Initialize the MISO, MOSI, and CLK pins and configure SPI instance
     */
    HardwareSPI(SPI_TypeDef *dSPI, uint32_t iBaudPresc,
                uint32_t iMosiPin, GPIO_TypeDef *iMosiPort, uint32_t iMosiAlternate,
                uint32_t iMisoPin, GPIO_TypeDef *iMisoPort, uint32_t iMisoAlternate,
                uint32_t iSckPin, GPIO_TypeDef *iSckPort, uint32_t iSckAlternate,
                IRQn_Type iSpiInterrupt, uint32_t iSpiPrePrio, uint32_t iSpiSubPrio,
                uint32_t iMode,
                uint32_t iClkPol, uint32_t iClkPhase,
                uint32_t iDataSize, uint32_t iNSS,
                SPI_HandleTypeDef *hSPI,
                uint32_t iSpiTimeOut)
            : RequestQueue(50, iSpiTimeOut), hSPI(hSPI) {
        AFIO(iMisoPin, iMisoPort, iMisoAlternate, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        AFIO(iMosiPin, iMosiPort, iMosiAlternate, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        AFIO(iSckPin, iSckPort, iSckAlternate, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);

        this->config(dSPI, iBaudPresc,
                     iSpiInterrupt, iSpiPrePrio, iSpiSubPrio,
                     iMode, iClkPol,
                     iClkPhase, iDataSize, iNSS);
    }

private:
    //\cond false
    inline static HardwareSPI *pThis = nullptr;
    SPI_HandleTypeDef *hSPI;

    void config(SPI_TypeDef *dSPI, uint32_t iBaudPresc,
                IRQn_Type iSpiInterrupt, uint32_t iSpiPrePrio, uint32_t iSpiSubPrio,
                uint32_t iMode,
                uint32_t iClkPol, uint32_t iClkPhase,
                uint32_t iDataSize, uint32_t iNSS) {
        *this->hSPI = {};
        this->hSPI->Instance = dSPI;
        this->hSPI->Init.Mode = iMode;
        this->hSPI->Init.Direction = SPI_DIRECTION_2LINES;
        this->hSPI->Init.DataSize = iDataSize;
        this->hSPI->Init.CLKPolarity = iClkPol;
        this->hSPI->Init.CLKPhase = iClkPhase;
        this->hSPI->Init.NSS = iNSS;
        this->hSPI->Init.BaudRatePrescaler = iBaudPresc;
        this->hSPI->Init.FirstBit = SPI_FIRSTBIT_MSB;
        this->hSPI->Init.TIMode = SPI_TIMODE_DISABLE;
        this->hSPI->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        while (HAL_SPI_Init(this->hSPI) != HAL_OK);

        HAL_SPI_RegisterCallback(this->hSPI, HAL_SPI_TX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(this->hSPI, HAL_SPI_RX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(this->hSPI, HAL_SPI_TX_RX_COMPLETE_CB_ID, transferComplete);

        HAL_NVIC_SetPriority(iSpiInterrupt, iSpiPrePrio, iSpiSubPrio);
        HAL_NVIC_EnableIRQ(iSpiInterrupt);
    }
    //\endcond
};

/**
 * helper function for copying data bit by bit into a struct
 * @param dest  struct address
 * @param numbits lenght of struct in bits
 * @param szof  sizeof struct
 * @param src   source buffer
 * @param len   number of structs to copy
 */
static void bitwisecopy(uint8_t *dest, size_t numbits, size_t szof, const uint8_t *src, size_t len) {
    uint32_t srcI = 0;
    for (size_t n = 0; n < len; ++n) {
        uint32_t destI = numbits;
        dest += n ? szof : 0;
        do {
            --destI;

            *(dest + destI / 8) &= ~(1U << destI % 8);
            *(dest + destI / 8) |= ((*(src + srcI / 8) >> (7 - srcI % 8)) & 0x1U) << destI % 8;

            ++srcI;
        } while (destI > 0);
    }
}

#endif //STM_SPI_H

