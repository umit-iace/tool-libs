/** @file Buffer.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <cstring>
#include <cstdint>

/**
 * dynamically allocated, but fixed-size buffer template
 *
 * keeps track of the number of items stored, and of allocated size
 * provides both copy & move semantics
 */
template<typename T>
struct Buffer {
    T *buf;
    size_t len, size;
    /** implicit cast to pointer to T */
    operator T*() { return buf; }
    //XXX: how about a slice into a buffer?

    /** access into known length of buffer */
    T& at(size_t ix) {
        assert(ix < len);
        return buf[ix];
    }
    /** access into known size of buffer */
    T& operator[](size_t ix) {
        assert(ix < size);
        return buf[ix];
    }
    /** simple append single item */
    Buffer& append(T b) {
        assert(len < size);
        buf[len++] = b;
        return *this;
    }
    /** begin method for range-based for loops */
    T* begin() { return &buf[0]; }
    T* begin() const { return &buf[0]; }
    /** end method for range-based for loops */
    T* end() { return &buf[len]; }
    T* end() const { return &buf[len]; }

    /* Rule of Five */
    /** destructor */
    ~Buffer() {
        log("Buffer del buf: %p\n", buf);
        delete[] buf; buf = nullptr; len = 0; size = 0;
    }
    /** copy from naked array with known length */
    Buffer(T *src, size_t len, size_t sz=0) : buf{}, len(len), size(sz) {
        if (sz == 0) size = len;
        buf = new T[size]();
        log("Buffer new from buf: %p len: %ld\n", src, sz);
        memcpy(buf, src, len);
    }
    /** constructor with fixed size */
    Buffer(size_t sz) : buf{new T[sz]()}, len(0), size(sz) {
        log("Buffer new buf: %p\n", buf);
    }
    /** copy constructor */
    Buffer(const Buffer &b) : buf{new T[b.size]()}, len(b.len), size(b.size) {
        memcpy(buf, b.buf, len);
        log("Buffer cp constr from: %p to: %p\n", b.buf, buf);
    }
    /** copy assignment operator */
    Buffer& operator=(const Buffer &b) {
        if (this == &b) return *this; // copy to self
        if (!size && size != b.size) { // necessary to realloc
            delete[] buf;
            buf = new T[b.size]();
            size = b.size;
        }
        assert(buf != nullptr);
        assert(size > b.len);
        log("Buffer cp from to: %p %p\n", b.buf, buf);
        memcpy(buf, b.buf, b.len);
        len = b.len;
        return *this;
    }
    /** move constructor */
    Buffer(Buffer &&b) noexcept : buf(b.buf), len(b.len), size(b.size) {
        log("Buffer mv constr from: %p to: %p\n", b.buf, buf);
        b.buf = nullptr;
        b.len = 0;
        b.size = 0;
    }
    /** move assignment operator */
    Buffer& operator=(Buffer &&b) noexcept {
        if (this == &b) return *this; // move to self
        log("Buffer mv from: %p to: %p\n", b.buf, buf);
        delete[] buf;
        buf = b.buf;
        len = b.len;
        size = b.size;
        b.len = 0;
        b.size = 0;
        b.buf = nullptr;
        return *this;
    }
};
