/** @file series.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "frameregistry.h"
/** unpacking helper for data series sent from pywisp
 *
 * series longer than MAXLEN elements are ignored
 */
template<typename T, int FRAMELEN=80, size_t MAXLEN=32768/sizeof(T)>
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
            // length comes from the other side, don't trust it blindly
            uint32_t len = f.unpack<uint32_t>();
            if (len == 0 || len > MAXLEN) return nullptr;
            buf = Buffer<T>(len);
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
