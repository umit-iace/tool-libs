#pragma once

#include "x/Kern.h"
#include "utils/Min.h"
#include "utils/Timeout.h"
#include "EventFuncRegistry.h"
#include "TimedFuncRegistry.h"
#include "FrameRegistry.h"
/** Experiment Controller
 *
 * implicitly depends on a Kernel object `k` in this namespace to be alive
 */
extern class Experiment {
public:
    enum State { IDLE, RUN };
    enum Event { INIT, STOP, TIMEOUT };
    Experiment() {
        k.every(1, *this, &Experiment::tick);
    }
    /** set frame registry from which Experiment will receive frames */
    void registerWith(FrameRegistry &reg) {
        reg.setHandler(1, *this, &Experiment::handleFrame);
    }
    /** register callbacks for Experiment state change events */
    Schedule::Evented::Registry& onEvent(Event e) {
        switch (e) {
            case INIT: return init;
            case STOP: return stop;
            default: return timeout;
        }
    }
    /** register regular callbacks during Experiment states */
    Schedule::Recurring::Registry& during(State s) {
        switch (s) {
            case IDLE: return idle;
            default: return running;
        }
    }
    /** const access to Experiment state */
    const State& state{state_};

    void setHeartbeatTimeout(uint32_t timeout) {
        heartbeat.timeout = timeout;
    }

private:
    State state_{};
    bool alive{};
    void statemachine() {
        State old = state;
        // very simple state machine
        state_ = alive ? RUN : IDLE;
        // react to state changes
        if (state != old) switch(old) {
        case IDLE:
            k.schedule(time, init);
            time = 0;
            idle.reset(); running.reset();
            break;
        case RUN:
            k.schedule(time, stop);
            break;
        } else switch (state) {
        // no state changes
        case IDLE:
            k.schedule(time, idle);
            break;
        case RUN :
           k.schedule(time, running);
           if (heartbeat && heartbeat(time)) {
               k.schedule(time, timeout);
               alive = false;
           }
           break;
        }
    }
    Schedule::Evented::Registry init{}, stop{}, timeout{};
    Schedule::Recurring::Registry idle{}, running{};
    uint32_t time{};
    struct : Timeout {
        uint32_t timeout{};
        operator bool() {
            return timeout;
        }
        void reset(uint32_t now) {
            when = now + timeout;
        }
    } heartbeat{};

    void tick(uint32_t, uint32_t dt) {
        statemachine();
        time += dt;
    }

    void handleFrame(Frame &f) {
        assert(f.id == 1);
        struct {
            uint8_t alive:1;
            uint8_t heartbeat:1;
            uint8_t _:6;
        } b = f.unpack<decltype(b)>();
        if (b.heartbeat && heartbeat) {
            heartbeat.reset(time);
        } else {
            alive = b.alive;
        }
    }
} e;
