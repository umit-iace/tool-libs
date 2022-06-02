/** @file spi.h
 *
 * Copyright (c) 2019 IACE
 *
 */

#ifndef STM_SPI_H
#define STM_SPI_H

#include <cstdint>

#include "stm/gpio.h"
#include "stm/hal.h"
#include "utils/RequestQueue.h"

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
    ChipSelect(DIO cs) :
            pin(cs) {
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
struct SPIRequest {
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
            HAL_SPI_Transmit_IT(&handle, rq->tData, rq->dataLen);
            break;
        case SPIRequest::MISO:
            HAL_SPI_Receive_IT(&handle, rq->rData, rq->dataLen);
            break;
        case SPIRequest::BOTH:
            HAL_SPI_TransmitReceive_IT(&handle, rq->tData, rq->rData, rq->dataLen);
            break;
        }
    }

    /**
     * override virtual RequestQueue function
     *
     * timeout -> abort
     */
     void rqTimeout(SPIRequest *rq) override {
         HAL_SPI_Abort_IT(&handle);
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
    struct Config {
        SPI_TypeDef *spi;
        uint32_t presc;
        uint8_t mode;
        uint8_t timeout;
        AFIO miso, mosi, sck;
    };
    HardwareSPI(
            Config conf
            ) : RequestQueue(50, conf.timeout) {
        handle = {
            .Instance = conf.spi,
            .Init = {
                .Mode = SPI_MODE_MASTER,
                .Direction = SPI_DIRECTION_2LINES,
                .DataSize = SPI_DATASIZE_8BIT,
                .NSS = SPI_NSS_SOFT,
                .BaudRatePrescaler = conf.presc,
                .FirstBit = SPI_FIRSTBIT_MSB,
                .TIMode = SPI_TIMODE_DISABLE,
                .CRCCalculation = SPI_CRCCALCULATION_DISABLE,
                .NSSPMode = SPI_NSS_PULSE_DISABLE,
            },
        };
        switch (conf.mode) {
            case 0:
                handle.Init.CLKPolarity = SPI_POLARITY_LOW;
                handle.Init.CLKPhase = SPI_PHASE_1EDGE;
                break;
            case 1:
                handle.Init.CLKPolarity = SPI_POLARITY_LOW;
                handle.Init.CLKPhase = SPI_PHASE_2EDGE;
                break;
            case 2:
                handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
                handle.Init.CLKPhase = SPI_PHASE_1EDGE;
                break;
            case 3:
                handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
                handle.Init.CLKPhase = SPI_PHASE_2EDGE;
                break;
            default:
                // there are only 4 modes.
                while(true) {};
        }
        while(HAL_SPI_Init(&handle) != HAL_OK);

        HAL_SPI_RegisterCallback(&handle, HAL_SPI_TX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(&handle, HAL_SPI_RX_COMPLETE_CB_ID, transferComplete);
        HAL_SPI_RegisterCallback(&handle, HAL_SPI_TX_RX_COMPLETE_CB_ID, transferComplete);

        pThis = this;
    }

    //\cond false
    inline static HardwareSPI *pThis = nullptr;
    SPI_HandleTypeDef handle{};
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
