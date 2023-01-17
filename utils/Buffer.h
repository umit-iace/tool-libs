#pragma once
#include <cstdio>
#include <cstring>
#include <cstdint>

struct Buffer {
    uint8_t *payload;
    size_t len, size;
    Buffer(size_t sz) : payload{new uint8_t[sz]()}, len(0), size(sz) {
        /* fprintf(stderr, "Buffer new payload: %p\n", payload); */
    }
    /** copy constructor */
    Buffer(const Buffer &b) : payload{new uint8_t[b.size]()}, len(b.len), size(b.size) {
        /* fprintf(stderr, "Buffer cp constr\n"); */
        memcpy(payload, b.payload, len);
        /* fprintf(stderr, "Buffer cpy payload: %p\n", payload); */
    }
    ~Buffer() {
        /* fprintf(stderr, "Buffer del payload: %p\n", payload); */
        delete[] payload;
    }
    /** move constructor */
    Buffer(Buffer &&b) : payload(b.payload), len(b.len), size(b.size) {
        /* fprintf(stderr, "Buffer mv constr\n"); */
        b.payload = 0;
        b.len = 0;
        b.size = 0;
    }
    /** move operator */
    Buffer& operator=(Buffer &&b) {
        delete[] payload;
        /* fprintf(stderr, "Buffer mv from to: %p %p\n", b.payload, payload); */
        payload = b.payload;
        len = b.len;
        size = b.size;
        b.len = 0;
        b.size = 0;
        b.payload = nullptr;
        return *this;
    }
    uint8_t& at(size_t ix) {
        assert(ix < len);
        return payload[ix];
    }
    uint8_t& operator[](size_t ix) {
        assert(ix < size);
        return payload[ix];
    }
    void append(uint8_t b) {
        assert(len < size);
        payload[len++] = b;
    }
};
