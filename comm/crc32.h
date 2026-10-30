/** @file crc32.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <stdint.h>

/** simple CRC32 implementation */
struct CRC32 {
    /** state */
    uint32_t checksum{0xffffffff};
    /** reinitialize state */
    void init() {
        *this = CRC32{};
    }
    /** calculate step with given byte
     *
     * a nibble at a time: 16 entry table (64 bytes), ~4x the bitwise speed
     */
    void step(uint8_t byte) {
        static constexpr uint32_t table[16] = {
            0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
            0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
            0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
            0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
        };
        checksum ^= byte;
        checksum = (checksum >> 4) ^ table[checksum & 0xf];
        checksum = (checksum >> 4) ^ table[checksum & 0xf];
    }
    /** get final CRC32 value */
    uint32_t finalize() {
        return ~checksum;
    }
};
