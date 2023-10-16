/** @file AS5145.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once

#include "stm/spi.h"
#include "utils/bitstream.h"
using namespace SPI;

/**
 * Class describing a chain of AS5145 Hall sensors
 */
template<int chainlength>
struct AS5145 : Device {
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
    AS5145(Sink<Request> &bus, DIO cs)
            : Device(bus, cs, {Mode::M2, FirstBit::MSB}) { }

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
        bus.push({
            .dev = this,
            .data = BYTES,
            .dir = Request::MISO,
            });
    }

    /*
     * callback when data is successfully measured
     *
     * copy data from buffer into struct.
     */
    void callback(const Request rq) override {
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
