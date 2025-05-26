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

    /** access into known size of buffer */
    T& operator[](size_t ix) const {
        assert(ix < len);
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
    Slice(const Buffer<T> &b, size_t start, size_t end=0) : buf{&b}, start(start) {
        len = end == 0 ? b.len - start : end - start;
        start = start;
        assert(end <= b.len);
    }
    Slice(Slice<T> slc, size_t start, size_t end=0) : buf{slc.buf}, start(slc.start + start) {
        len = end == 0 ? slc.buf->len - slc.start - start : end - start;
        assert(end <= slc.buf->len - slc.start);
    }
};
