/** @file series.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "frameregistry.h"
/** unpacking helper for data series sent from pywisp */
template<typename T, int FRAMELEN=80>
struct SeriesUnpacker {
    static constexpr size_t LEN = FRAMELEN / sizeof(T);
    bool start {true};
    Buffer<T> buf; ///< Buffer of data received. only
                   ///valid after unpack returns non-null

    /** call this with incoming frames until it returns
     * a Buffer filled with the data sent over the wire.
     */
    Buffer<T> *unpack(Frame &f) {
        if (start) {
            buf = f.unpack<uint32_t>();
            if (buf.size == 0) return nullptr;
        }
        for (unsigned int i = 0; i < LEN - start; i++) {
            buf.append(f.unpack<T>());
            if (buf.len == buf.size) {
                start = true;
                return &buf;
            }
        }
        start = false;
        return nullptr;
    }
};
