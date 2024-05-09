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

/** parse command line argument for simulation time factor.
 * Factor must be in range [1, 1000].
 * returns the necessary simulation time step in `us` for passing
 * directly to `k.setTimeStep()`
 */
static uint16_t getSimTimeStep(int argc, char *argv[]) {
    switch (argc) {
        case 1: 
            k.log.info("using default simulation time step = 1ms\n");
            return 1000;
        case 2: break;
        default:
            k.log.warn("usage: %s [time_factor]\n", argv[0]);
            exit(EXIT_FAILURE);
    }
    uint16_t factor = strtol(argv[1],nullptr,10);
    if (factor < 1 || factor > 1000) {
        k.log.warn("factor _must_ be in range [1, 1000]\n");
        exit(EXIT_FAILURE);
    }
    uint16_t ret = 1000/factor;
    k.log.info("using simulation time step = %dus\n", ret);
    return ret;
}
