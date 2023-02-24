#pragma once

#include "x/Kern.h"
#include "utils/Timeout.h"
#include "EventFuncRegistry.h"
#include "FrameRegistry.h"

/** @brief Experiment Controller
 *
 * Implements a simple state machine and provides hooks for regular callbacks
 * during the states, or on Events (state changes).
 * \dot
 * digraph Experiment_State_Machine {
 *      rankdir=LR;
 *      size="8,5"
 *      node [shape = circle];
 *      IDLE [URL="\ref IDLE"]
 *      RUN [URL="\ref RUN"]
 *      IDLE -> IDLE [label= "tick"]
 *      IDLE -> RUN [ label = "INIT", URL="\ref INIT"];
 *      RUN -> IDLE [ label = "STOP", URL="\ref STOP"];
 *      RUN -> IDLE [ label = "TIMEOUT", URL="\ref TIMEOUT"];
 *      RUN -> RUN [label= "tick"]
 * }
 * \enddot
 * \note implicitly depends on a Kernel object `k` in this namespace to be alive
 */
extern class Experiment {
public:
    /// States during which recurring tasks can be called using \ref during
    enum State { 
        /// initial Experiment state
        IDLE,
        /// running Experiment state
        RUN,
    };
    /// Events upon which tasks can be called using \ref onEvent
    enum Event {
        /// Event generated on experiment start
        INIT,
        /// Event generated on experiment end
        STOP,
        /// Event generated on missed heartbeat
        TIMEOUT,
    };
    Experiment() {
        k.every(1, *this, &Experiment::tick);
    }
    /// set frame registry from which Experiment will receive frames
    void registerWith(FrameRegistry &reg) {
        reg.setHandler(1, *this, &Experiment::handleFrame);
    }
    /// register callbacks for Experiment state change events
    Schedule::Evented::Registry& onEvent(Event e) {
        switch (e) {
            case INIT: return init;
            case STOP: return stop;
            default: return timeout;
        }
    }
    /// register recurring callbacks during Experiment states
    Schedule::Recurring::Registry& during(State s) {
        switch (s) {
            case IDLE: return idle;
            default: return running;
        }
    }
    /// const access to Experiment state
    const State& state{state_};
    /// set Heartbeat timeout in ms
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
    class : Timeout {
    friend Experiment;
        uint32_t timeout{};
        operator bool() {
            return timeout;
        }
        bool reset(uint32_t now) {
            when = now + timeout;
            return true;
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
        if (b.heartbeat) {
            (bool)heartbeat && heartbeat.reset(time);
        } else {
            alive = b.alive;
        }
    }
} e;
