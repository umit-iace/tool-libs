#pragma once
#include "Min.h"

struct Experiment {
    enum State { IDLE, INIT, RUN, STOP } s{};
    bool alive{};
    void stop() {
        alive = false;
    }
    operator enum State() {
        return s;
    }
};

void ExperimentHandler(Frame &f, Experiment &e) {
    assert(f.id == 1);
    uint8_t b;
    f.unPack(b);
    if (b & 2) {
        // heartbeat.reset()
        /* pExp->lHeartBeatLast = pExp->lTime; */
    } else {
        e.alive = b&1;
    }
}

