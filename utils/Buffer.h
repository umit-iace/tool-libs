#pragma once
#include <cstring>
#include <cstdint>
#ifndef DEBUGLOG
#define DEBUGLOG(...)
#define UNDEF
#endif

template<typename T>
struct Buffer {
    T *payload;
    size_t len, size;

    /** access */
    T& at(size_t ix) {
        assert(ix < len);
        return payload[ix];
    }
    T& operator[](size_t ix) {
        assert(ix < size);
        return payload[ix];
    }
    Buffer& append(T b) {
        assert(len < size);
        payload[len++] = b;
        return *this;
    }
    T* begin() {
        return &payload[0];
    }
    T* end() {
        return &payload[len];
    }
    /* Rule of Five */
    /** destructor */
    ~Buffer() {
        DEBUGLOG(stderr, "Buffer del payload: %p\n", payload);
        delete[] payload; len = 0; size = 0;
    }
    /** constructor */
    Buffer(size_t sz) : payload{new T[sz]()}, len(0), size(sz) {
        DEBUGLOG(stderr, "Buffer new payload: %p\n", payload);
    }
    /** copy constructor */
    Buffer(const Buffer &b) : payload{new T[b.size]()}, len(b.len), size(b.size) {
        DEBUGLOG(stderr, "Buffer cp constr\n");
        memcpy(payload, b.payload, len);
        DEBUGLOG(stderr, "Buffer cp constr from: %p to: %p\n", b.payload, payload);
    }
    /** copy assignment operator */
    Buffer& operator=(const Buffer &b) {
        if (this == &b) return *this; // copy to self
        if (!size && size != b.size) { // necessary to realloc
            delete[] payload;
            payload = new T[b.size]();
            size = b.size;
        }
        assert(payload != nullptr);
        assert(size > b.len);
        DEBUGLOG(stderr, "Buffer cp from to: %p %p\n", b.payload, payload);
        memcpy(payload, b.payload, b.len);
        len = b.len;
        return *this;
    }
    /** move constructor */
    Buffer(Buffer &&b) noexcept : payload(b.payload), len(b.len), size(b.size) {
        DEBUGLOG(stderr, "Buffer mv constr from: %p to: %p\n", b.payload, payload);
        b.payload = 0;
        b.len = 0;
        b.size = 0;
    }
    /** move assignment operator */
    Buffer& operator=(Buffer &&b) noexcept {
        if (this == &b) return *this; // move to self
        DEBUGLOG(stderr, "Buffer mv from to: %p %p\n", b.payload, payload);
        delete[] payload;
        payload = b.payload;
        len = b.len;
        size = b.size;
        b.len = 0;
        b.size = 0;
        b.payload = nullptr;
        return *this;
    }
};
#ifdef UNDEF
#undef DEBUGLOG
#endif
