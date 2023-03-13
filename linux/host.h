#pragma once
#include <core/streams.h>
#include <utils/buffer.h>
extern struct Host {
    struct {
        Source<Buffer<uint8_t>> &in;
        Sink<Buffer<uint8_t>> &out;
    } socket, tty;
} host;
