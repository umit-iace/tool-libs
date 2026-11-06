/** @file buffer.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <cstring>
#include <cstdint>
#include <cassert>
#include <initializer_list>
#include <type_traits>

/**
 * dynamically allocated, but fixed-size buffer template
 *
 * keeps track of the number of items stored, and of allocated size
 * provides both copy & move semantics
 */
template<typename T>
struct Buffer {
    T *buf;
    /** number of items stored in buffer */
    size_t len;
    /** total capacity of buffer */
    size_t size;
    /** implicit cast to pointer to T */
    operator T*() { return buf; }
    operator const T*() const { return buf; }

    /** access into known length of buffer */
    T& at(size_t ix) {
        assert(ix < len);
        return buf[ix];
    }
    const T& at(size_t ix) const {
        assert(ix < len);
        return buf[ix];
    }
    /** access into known size of buffer
     *
     * the index is a template: an exact match for every integer type, otherwise
     * e.g. buf[1] (int) is ambiguous with the built-in index of the implicit
     * cast to T* (ISO C++, warned by gcc where size_t is unsigned int)
     */
    template<typename I, typename = std::enable_if_t<std::is_integral_v<I>>>
    T& operator[](I ix) {
        assert(inSize(ix));
        return buf[ix];
    }
    /** access into known size of buffer */
    template<typename I, typename = std::enable_if_t<std::is_integral_v<I>>>
    const T& operator[](I ix) const {
        assert(inSize(ix));
        return buf[ix];
    }
    /** index within the allocated size, negative ones are not */
    template<typename I>
    bool inSize(I ix) const {
        if constexpr (std::is_signed_v<I>) {
            if (ix < 0) return false;
        }
        return static_cast<size_t>(ix) < size;
    }
    /** simple append single item
     *
     * items not fitting are dropped, also in release builds (NDEBUG)
     */
    Buffer& append(T b) {
        assert(len < size);
        if (len >= size) return *this;
        buf[len++] = b;
        return *this;
    }
    /** append multiple items */
    Buffer& append(std::initializer_list<T> list) {
        for (auto v: list) append(v);
        return *this;
    }
    /** begin method for range-based for loops */
    T* begin() { return buf; }
    const T* begin() const { return buf; }
    /** end method for range-based for loops */
    T* end() { return buf + len; }
    const T* end() const { return buf + len; }

    /* Rule of Five */
    /** destructor */
    ~Buffer() {
        delete[] buf; buf = nullptr; len = 0; size = 0;
    }
    /** copy from naked array with known length */
    Buffer(const T *src, size_t len, size_t sz=0) : buf{}, len(0), size(sz) {
        if (sz == 0) size = len;
        buf = alloc(size);
        for (size_t i = 0; i < len; ++i) append(src[i]);
    }
    /** initializer list constructor */
    Buffer(std::initializer_list<T> list) : buf{alloc(list.size())}, len(0), size(list.size()) {
        append(list);
    }
    /** empty Buffer, no allocation */
    Buffer() : buf{}, len(0), size(0) { }
    /** constructor with fixed size
     *
     * explicit: `push(5)` must not quietly become a Buffer of 5 elements
     */
    explicit Buffer(size_t sz) : buf{alloc(sz)}, len(0), size(sz) { }
    /** copy constructor */
    Buffer(const Buffer &b) : buf{alloc(b.size)}, len(0), size(b.size) {
        for (auto el : b) append(el);
    }
    /** copy assignment operator */
    Buffer& operator=(const Buffer &b) {
        if (this == &b) return *this; // copy to self
        if (!size || size != b.size) { // necessary to realloc
            delete[] buf;
            buf = alloc(b.size);
            size = b.size;
        }
        assert(size >= b.len);
        len = 0;
        for (auto el: b) append(el);
        return *this;
    }
    /** move constructor */
    Buffer(Buffer &&b) noexcept : buf(b.buf), len(b.len), size(b.size) {
        b.buf = nullptr;
        b.len = 0;
        b.size = 0;
    }
    /** move assignment operator */
    Buffer& operator=(Buffer &&b) noexcept {
        if (this == &b) return *this; // move to self
        delete[] buf;
        buf = b.buf;
        len = b.len;
        size = b.size;
        b.len = 0;
        b.size = 0;
        b.buf = nullptr;
        return *this;
    }
private:
    static T *alloc(size_t n) { return n ? new T[n] : nullptr; }
};
