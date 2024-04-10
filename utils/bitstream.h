/** @file bitstream.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <stdint.h>

/**
 * Wrapper for extracting bitwise data from a stream of bytes.
 * Assumes Most Significant Bit first, Most Significant Byte first.
 */
struct BitStream {
    uint8_t *base; ///< pointer to first byte of stream
    /** return bitrange from stream as T */
    template<typename T>
    T range(int start, int end) {
        T dest{};
        int len = end-start;
        int i = 1;
        while (start < end) {
            dest |= (base[start/8] & 0x80>>start%8) >> (7 - start%8) << (len - i);
            ++start;
            ++i;
        }
        return dest;
    }
};
