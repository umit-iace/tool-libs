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
    enum Event { INIT, STOP };
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
            default: return stop;
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
        }
    }
    Schedule::Evented::Registry init{}, stop{};
    Schedule::Recurring::Registry idle{}, running{};
    uint32_t time{};
    Timeout heartbeat{};
    uint32_t hb_timeout{500};

    void tick(uint32_t, uint32_t dt) {
        statemachine();
        if (heartbeat(time)) alive = false;
        switch (state) {
            case IDLE: k.schedule(time, idle); break;
            case RUN : k.schedule(time, running); break;
            default:;
        }
        time += dt;
    }

    void handleFrame(Frame &f) {
        assert(f.id == 1);
        struct {
            uint8_t alive:1;
            uint8_t heartbeat:1;
            uint8_t _:6;
        } b = f.unpack<decltype(b)>();
        if (b.heartbeat) {
            heartbeat = {time + hb_timeout};
        } else {
            alive = b.alive;
        }
    }
} e;
