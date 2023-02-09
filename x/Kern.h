#pragma once

#include "Experiment.h"
#include "utils/Timeout.h"
#include "EventFuncRegistry.h"
#include "TimedFuncRegistry.h"
#include "Schedule.h"
#define HB (500)
inline struct Experiment {
    enum State { IDLE, RUN } state{};
    bool alive{};
    void statemachine() {
        State old = state;
        // very simple state machine
        state = alive ? RUN : IDLE;
        // react to state changes
        if (state != old) switch(old) {
        case IDLE:
            init.schedule(s,time);
            time = 0;
            always.reset(); idle.reset(); running.reset();
            break;
        case RUN:
            stop.schedule(s,time);
            break;
        }
    }
    Timeout heartbeat{};
    EventFuncRegistry init{}, stop{};
    TimedFuncRegistry always{}, idle{}, running{};
    Scheduler s{};
    uint32_t time{};

    /** call this every millisecond in an interrupt */
    void ms_tick(uint32_t globaltime) {
        /* time = globaltime - expstarttime; */
        statemachine();
        if (heartbeat(time)) alive = false;
        always.schedule(s,time);
        switch (state) {
            case IDLE: idle.schedule(s,time); break;
            case RUN: running.schedule(s,time); break;
            default:;
        }
        time++;
    }
    /** call this in the main loop to actually run the functions
     * previously scheduled in ``ms_tick``
     */
    void run() {
        s.run();
    }

    void handleFrame(Frame &f, uint32_t time_ms) {
        assert(f.id == 1);
        struct {
            uint8_t alive:1;
            uint8_t heartbeat:1;
            uint8_t _:6;
        } b = f.unpack<decltype(b)>();
        if (b.heartbeat) {
            heartbeat = {time_ms + HB};
        } else {
            alive = b.alive;
        }
    }
} k;
