/** @file AS5145.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once

#include "sys/spi.h"
#include "utils/bitstream.h"

/** Class describing a chain of AS5145 Hall sensors */
template<int chainlength>
struct AS5145 : SPI::Device {
    static constexpr uint8_t BITS = 19; // number of bits in sensor data struct
    static constexpr uint8_t TOTALBITS = BITS*chainlength; // number of bits in sensor data struct
    // total bytes required for chain
    static constexpr uint8_t BYTES = (TOTALBITS+7) / 8;
    // sensor data struct. lsb to msb order.
    struct sensor {
        uint8_t EVEN:1;
        uint8_t DEC:1;
        uint8_t INC:1;
        uint8_t LIN:1;
        uint8_t COF:1;
        uint8_t OCF:1;
        int16_t POS:12;
        uint8_t :1;
    } chain[chainlength];

    /**
     * Constructor
     * @param bus SPI bus
     * @param cs ChipSelect pin
     */
    AS5145(Sink<SPI::Request> &bus, DIO cs)
            : SPI::Device(bus, cs, {SPI::Mode::M2, SPI::FirstBit::MSB}) { }

    /**
     * @param i index of sensor in chain
     * @return measured sensor angle
     */
    int16_t getVal(int i = 0) {
        return chain[i].POS;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        bus.trypush({
            .dev = this,
            .data = BYTES,
            .dir = SPI::Request::MISO,
            });
    }

    /*
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(const SPI::Request rq) override {
        BitStream bs{rq.data.buf};
        for (int i = 0, n=0; i < chainlength; ++i, n+=BITS) {
            chain[i].COF = bs.range<decltype(sensor::COF)>(n + 14, n + 15);
            if (chain[i].COF) { // out of range error
                continue;
            }
            chain[i].POS = bs.range<decltype(sensor::POS)>(n + 1, n + 13);
        }
    }
};
