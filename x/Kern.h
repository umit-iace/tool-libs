#pragma once

#include "Experiment.h"
#include "utils/Timeout.h"
#include "EventFuncRegistry.h"
#include "TimedFuncRegistry.h"
#include "Schedule.h"
#define HB (500)
inline struct Experiment {
    enum State { IDLE, INIT, RUN, STOP } state{};
    bool alive{};
    Timeout keeper{};
    void statemachine() {
        State old = state;
        switch (state) {
        case IDLE:
            state = alive ? INIT : IDLE;
            break;
        case INIT:
            state = keeper(time)? RUN : INIT;
            state = alive ? state : STOP;
            break;
        case RUN:
            state = alive ? RUN : STOP;
            break;
        case STOP:
            state = keeper(time)? IDLE : STOP;
            break;
        }
        if (state != old) switch(state) {
        case IDLE:
            break;
        case INIT:
            keeper = {time+400};
            init.schedule(s,time);
            break;
        case RUN:
            time = 0;
            always.reset(); idle.reset(); running.reset();
            break;
        case STOP:
            keeper = {time+100};
            stop.schedule(s,time);
            break;
        }
    }
    Timeout heartbeat{};
    EventFuncRegistry init{}, stop{};
    TimedFuncRegistry always{}, idle{}, running{};
    Scheduler s{};
    uint32_t time{};
    /* Buffer<Scheduler*> sched{5}; */
    /* Experiment() { */
    /*     sched.append(&stop).append(&init).append(&running).append(&idle).append(&always); */
    /* } */

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
        /* init.run(); */
        /* stop.run(); */
        /* always.run(); */
        /* idle.run(); */
        /* running.run(); */
        /* for (auto s: sched) { */
        /*     s->run(); */
        /* } */
    }

    void handleFrame(Frame &f, uint32_t time_ms) {
        assert(f.id == 1);
        uint8_t b;
        f.unPack(b);
        if (b & 2) {
            heartbeat = {time_ms + HB};
        } else {
            alive = b&1;
        }
    }
} k;
