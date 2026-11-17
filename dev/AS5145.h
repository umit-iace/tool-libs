/** @file AS5145.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once

#include "sys/spi.h"
#include "utils/bitstream.h"
#include <bit>

/** Class describing a chain of AS5145 Hall sensors */
template<int chainlength>
struct AS5145 : SPI::Device {
    static_assert(chainlength > 0);
    static constexpr size_t BITS = 19; // number of bits in sensor data struct
    static constexpr size_t TOTALBITS = BITS*chainlength; // number of bits in sensor data struct
    // total bytes required for chain
    static constexpr size_t BYTES = (TOTALBITS+7) / 8;
    /** status bits as returned by flags() */
    enum : uint8_t {
        OCF = 1 << 0, ///< offset compensation finished
        COF = 1 << 1, ///< cordic overflow
        LIN = 1 << 2, ///< linearity alarm
        INC = 1 << 3, ///< magnitude increase
        DEC = 1 << 4, ///< magnitude decrease
        PARITY_OK = 1 << 5, ///< even parity check passed
    };
    /** last read of one sensor */
    struct sensor {
        uint16_t POS; ///< last valid position
        uint8_t flags; ///< status bits of last read
    } chain[chainlength]{};
    uint32_t reads = 0; ///< number of completed chain reads

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
    uint16_t getVal(int i = 0) {
        return chain[i].POS;
    }

    /**
     * @param i index of sensor in chain
     * @return true if last read of sensor i was error free
     */
    bool valid(size_t i) const {
        uint8_t f = chain[i].flags;
        return (f & OCF) && !(f & (COF | LIN)) && (f & (INC | DEC)) != (INC | DEC) && (f & PARITY_OK);
    }

    /**
     * @param i index of sensor in chain
     * @return raw status bits of last read, see OCF..PARITY_OK
     */
    uint8_t flags(size_t i) const {
        return chain[i].flags;
    }

    /**
     * request measurement of sensor data
     */
    void sense() {
        bus.trypush({
            .dev = this,
            .data = Buffer<uint8_t>(BYTES),
            .dir = SPI::Request::MISO,
            });
    }

    /*
     * callback when data is successfully measured
     *
     * copy status from buffer into struct, position only if valid.
     */
    void callback(const SPI::Request &rq) override {
        BitStream bs{rq.data.buf};
        ++reads;
        // per sensor: dummy bit, POS:12, OCF, COF, LIN, INC, DEC, even parity
        for (size_t i = 0, n = 0; i < chainlength; ++i, n += BITS) {
            // parity bit makes the number of ones over the 18 bits POS..parity even
            uint8_t f = std::popcount(bs.range<uint32_t>(n + 1, n + 19)) % 2 ? 0 : PARITY_OK;
            for (size_t b = 0; b < 5; ++b) { // OCF..DEC -> bit0..4
                f |= bs.range<uint8_t>(n + 13 + b, n + 14 + b) << b;
            }
            chain[i].flags = f;
            if (valid(i)) {
                chain[i].POS = bs.range<uint16_t>(n + 1, n + 13);
            }
        }
    }
};
