#pragma once
#include <core/kern.h>
#include <core/streams.h>
#include <cstdlib>
extern struct Host {
    struct {
        Source<Buffer<uint8_t>> &in;
        Sink<Buffer<uint8_t>> &out;
    } socket, tty;
} host;

