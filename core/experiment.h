/** @file experiment.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <comm/frameregistry.h>
#include <utils/deadline.h>

#include "kern.h"
#include "evented.h"
#include "logger.h"

/** @brief Experiment Controller
 *
 * Implements a simple state machine and provides hooks for recurring callbacks
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
    struct ELog : Logger {
        const uint32_t &time;
        virtual Buffer<uint8_t> pre() override {
            auto ret = Buffer<uint8_t>{256};
            ret.len = snprintf((char*)ret.buf, ret.size,
                    "Experiment Logger (@%ldms): ", time);
            return ret;
        }
        ELog(const uint32_t &time, Sink<Buffer<uint8_t>> &underlying)
                : Logger(underlying), time(time) {
        }
        void start(uint32_t) { info("started\n"); }
        void stop(uint32_t) { info("stopped\n"); }
        void timeout(uint32_t) { warn("timed out!\n"); }
    };
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
    /// construct. if FrameRegistry is not available at this point,
    /// call `registerWith` at the earliest convenience
    Experiment(FrameRegistry *fr=nullptr) {
        k.every(1, *this, &Experiment::tick);
        if (fr) registerWith(*fr);
        onEvent(INIT).call(log, &ELog::start);
        onEvent(STOP).call(log, &ELog::stop);
        onEvent(TIMEOUT).call(log, &ELog::timeout);
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
    /// const access to Experiment time
    const uint32_t& time{time_};
    /// set Heartbeat timeout in ms
    void setHeartbeatTimeout(uint32_t ms, Sink<Frame> &notify) {
        heartbeat.timeout = ms;
        heartbeat.reset(time);
        // notify other side if we timeout
        struct TMP :Schedule::Schedulable{
                Sink<Frame> &s;
                TMP(Sink<Frame>&s): s(s) {}
                void call() override{
                    s.push(Frame{1}.pack(false));
                }};
        timeout.call(new TMP{notify});
    }
    /// Logging facility
    ELog log{time, k.log};

private:
    State state_{};
    bool alive{};
    void statemachine() {
        State old = state;
        // very simple state machine
        state_ = alive ? RUN : IDLE;
        if (state != old) switch(old) { // react to state changes
        case IDLE:
            time_ = 0;
            k.schedule(time, init);
            idle.reset(); running.reset();
            break;
        case RUN:
            k.schedule(time, stop);
            break;
        } else switch (state) { // no state changes
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
    uint32_t time_{};
    class _: Deadline {
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
        time_ += dt;
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

