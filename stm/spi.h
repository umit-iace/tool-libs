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

#if defined(HW_SPI_MODE_0)
#define HW_SPI_CLK_POLARITY SPI_POLARITY_LOW
#define HW_SPI_CLK_PHASE SPI_PHASE_1EDGE
#elif defined(HW_SPI_MODE_1)
#define HW_SPI_CLK_POLARITY SPI_POLARITY_LOW
#define HW_SPI_CLK_PHASE SPI_PHASE_2EDGE
#elif defined(HW_SPI_MODE_2)
#define HW_SPI_CLK_POLARITY SPI_POLARITY_HIGH
#define HW_SPI_CLK_PHASE SPI_PHASE_1EDGE
#elif defined(HW_SPI_MODE_3)
#define HW_SPI_CLK_POLARITY SPI_POLARITY_HIGH
#define HW_SPI_CLK_PHASE SPI_PHASE_2EDGE
#endif

//\cond false
extern "C" {
SPI_HandleTypeDef hHWSPI;
};
//\endcond

/**
 * Class abstracting an SPI device.
 * Implement your SPI device with this as a base class, then you can use the
 * HardwareSPI to send/get data over the wire.
 */
class ChipSelect {
    //\cond false
    DIO pin;
    //\endcond
public:
    /**
     * @param iCSPIN chip select pin number
     * @param gpioCSPort chip select pin port
     */
    ChipSelect(uint32_t iCSPIN, GPIO_TypeDef *gpioCSPort) :
            pin(iCSPIN, gpioCSPort) {
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
    virtual void callback(void *cbData) { }
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
};

/**
 * @brief Template class for hardware based SPI derivations
 * @todo make it possible to use more than one HardwareSPI at the same time
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
    void rqBegin(SPIRequest *rq) override {
        // activate the chip
        rq->cs->selectChip(true);

        // transfer the data
        switch (rq->dir) {
        case SPIRequest::MOSI:
            HAL_SPI_Transmit_IT(&this->hSPI, rq->tData, rq->dataLen);
            break;
        case SPIRequest::MISO:
            HAL_SPI_Receive_IT(&this->hSPI, rq->rData, rq->dataLen);
            break;
        case SPIRequest::BOTH:
            HAL_SPI_TransmitReceive_IT(&this->hSPI, rq->tData, rq->rData, rq->dataLen);
            break;
        }
    }

    /**
     * override virtual RequestQueue function
     *
     * timeout -> abort
     */
     void rqTimeout(SPIRequest *rq) override {
         HAL_SPI_Abort_IT(&this->hSPI);
         rq->cs->selectChip(false);
         rqEnd();
     }

private:
    /**
     * complete the data transfer, signal request completion
     * @param hspi
     */
    static void transferComplete(SPI_HandleTypeDef *hspi) {
        auto r = pThis->rqCurrent();
        // deactivate chip
        r->cs->selectChip(false);
        // signal data arrival
        if (r->cbData) {
            r->cs->callback(r->cbData);
        }
        // signal request complete
        pThis->rqEnd();
    }

public:
    static HardwareSPI *master() {
        if (!pThis) {
            pThis = new HardwareSPI();
        }
        return pThis;
    }

private:
    /**
     * Initialize the MISO, MOSI, and CLK pins and configure SPI instance
     */
    HardwareSPI() : RequestQueue(50, HW_SPI_TIMEOUT) {
        AFIO(HW_SPI_MISO_PIN, HW_SPI_MISO_PORT, HW_SPI_MISO_ALTERNATE,
            GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        AFIO(HW_SPI_MOSI_PIN, HW_SPI_MOSI_PORT, HW_SPI_MOSI_ALTERNATE,
            GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        AFIO(HW_SPI_SCK_PIN, HW_SPI_SCK_PORT, HW_SPI_SCK_ALTERNATE,
            GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
#if !defined(HW_SPI) || !defined(HW_SPI_BAUD_PRESCALER) ||\
    !defined(HW_SPI_CLK_POLARITY) || !defined(HW_SPI_CLK_PHASE)
#error "you have not set all necessary configuration defines!"
#endif
        this->config(HW_SPI, HW_SPI_BAUD_PRESCALER,
                HW_SPI_CLK_POLARITY, HW_SPI_CLK_PHASE);
    }

    //\cond false
    inline static HardwareSPI *pThis = nullptr;
    SPI_HandleTypeDef &hSPI = hHWSPI;

    void config(SPI_TypeDef *dSPI, uint32_t iBaudPresc,
            uint32_t iClkPol, uint32_t iClkPhase) {
        hSPI.Instance = dSPI;
        hSPI.Init.Mode = SPI_MODE_MASTER;
        hSPI.Init.Direction = SPI_DIRECTION_2LINES;
        hSPI.Init.DataSize = SPI_DATASIZE_8BIT;
        hSPI.Init.CLKPolarity = iClkPol;
        hSPI.Init.CLKPhase = iClkPhase;
        hSPI.Init.NSS = SPI_NSS_SOFT;
        hSPI.Init.BaudRatePrescaler = iBaudPresc;
        hSPI.Init.FirstBit = SPI_FIRSTBIT_MSB;
        hSPI.Init.TIMode = SPI_TIMODE_DISABLE;
        hSPI.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        hSPI.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        while(HAL_SPI_Init(&hSPI) != HAL_OK);

        HAL_SPI_RegisterCallback(&hSPI, HAL_SPI_TX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(&hSPI, HAL_SPI_RX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(&hSPI, HAL_SPI_TX_RX_COMPLETE_CB_ID, transferComplete);

        HAL_NVIC_SetPriority(SPI_IT_IRQn, SPI_IT_PRIO);
        HAL_NVIC_EnableIRQ(SPI_IT_IRQn);
    }
    //\endcond
};

/**
 * helper function for copying data bit by bit into a struct
 * @param dest  struct address
 * @param src   source buffer
 * @param numbits length of struct in bits
 * @param len   number of structs to copy
 */
static void bitwisecopy(uint8_t *dest, const uint8_t *src, size_t numbits, size_t len) {
    uint32_t srcI = 0;
    size_t szof = (numbits + 7) / 8;
    for (size_t n = 0; n < len; ++n) {
        dest += n?szof:0;
        for (int destI = numbits - 1; destI >= 0; --destI, ++srcI) {
            dest[destI / 8] &= ~(1U << destI % 8);
            dest[destI / 8] |= ((src[srcI / 8] >> (7 - srcI % 8)) & 0x1U) << destI % 8;
        }
    }
}

/**
 * helper function for changing endianness of a buffer
 * @param buffer address of buffer
 * @param len length of buffer
 */
static void flip(uint8_t *buffer, size_t len) {
    uint8_t tmp;
    for (size_t i = 0; i < len / 2; ++i) {
        tmp = buffer[i];
        buffer[i] = buffer[len - 1 - i];
        buffer[len - 1 - i] = tmp;
    }
}

#endif //STM_SPI_H
