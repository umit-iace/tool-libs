/** @file slice.h
 *
 * Copyright (c) 2025 IACE
 */
#pragma once
#include "buffer.h"

/**
 * slice of existing Buffer. Does _NOT_ own, is view
 */
template<typename T>
struct Slice {
    /** pointer to owning Buffer */
    const Buffer<T> *buf;
    /** number of items in slice */
    size_t len;
    size_t start;

    /** access into known size of buffer
     *
     * out of range reads give a default T, also in release builds (NDEBUG)
     */
    T& operator[](size_t ix) const {
        assert(ix < len);
        if (ix >= len) {
            static T outside;
            outside = T{};
            return outside;
        }
        return buf->buf[start+ix];
    }
    /** begin method for range-based for loops */
    T* begin() const { return &buf->buf[start]; }
    /** end method for range-based for loops */
    T* end() const { return &buf->buf[start+len]; }

    /** destructor */
    ~Slice() {
        buf = nullptr; len = 0;
    }
    /** view of b from start to end (exclusive), end = 0: up to b.len
     *
     * clamped to the data in b: a start beyond it gives an empty Slice
     */
    Slice(const Buffer<T> &b, size_t start, size_t end=0) : buf{&b} {
        assert(end <= b.len);
        clamp(0, b.len, start, end);
    }
    /** view into slc, start & end relative to slc, see above */
    Slice(Slice<T> slc, size_t start, size_t end=0) : buf{slc.buf} {
        assert(end <= slc.len);
        clamp(slc.start, slc.len, start, end);
    }
private:
    void clamp(size_t base, size_t avail, size_t from, size_t to) {
        if (to == 0 || to > avail) to = avail;
        if (from > to) from = to;
        start = base + from;
        len = to - from;
    }
public:
};
