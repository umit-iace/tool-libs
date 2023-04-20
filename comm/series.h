/** @file series.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "frameregistry.h"
/** unpacking helper for data series sent from pywisp */
struct SeriesUnpacker {
    bool start {true};
    Buffer<double> buf{0};

    /** call this with incoming frames until it returns
     * a Buffer filled with the data sent over the wire
     */
    Buffer<double> *unpack(Frame &f) {
        constexpr unsigned int LEN = 40 / 8;

        if (start) {
            buf = Buffer<double>{f.unpack<uint32_t>()};
        }
        for (unsigned int i = 0; i < LEN - start; i++) {
            buf.append(f.unpack<double>());
            if (buf.len == buf.size) {
                start = true;
                return &buf;
            }
        }
        start = false;
        return nullptr;
    }
};
