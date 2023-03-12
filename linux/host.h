#pragma once
#include <cassert>
#include <utils/Interfaces.h>
#include <utils/Buffer.h>
extern struct Host {
    struct {
        Source<Buffer<uint8_t>> &in;
        Sink<Buffer<uint8_t>> &out;
    } socket, tty;
} host;
