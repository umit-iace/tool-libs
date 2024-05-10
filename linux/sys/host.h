#pragma once
#warning usage of <sys/host.h> has been deprecated.
#warning make use of <sys/comm.h> and/or <sys/sim.h> instead
#include "comm.h"
#include "sim.h"

inline TTY tty {"/dev/tty"};
inline UDP udp {"127.0.0.1", 45670};
inline struct Host {
    struct {
        Source<Buffer<uint8_t>> &in;
        Sink<Buffer<uint8_t>> &out;
    } socket, tty;
} host {
    .socket = {
        .in = udp,
        .out = udp,
    },
    .tty = {
        .in = tty,
        .out = tty,
    },
};

