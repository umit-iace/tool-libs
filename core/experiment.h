/** @file experiment.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <comm/crc32.h>
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
            Buffer<uint8_t> ret(256);
            ret.len = snprintf((char*)ret.buf, ret.size,
                    "Experiment Logger (@%" PRIu32 "ms): ", time);
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
        ticker = k.every(1, *this, &Experiment::tick);
        if (fr) registerWith(*fr);
        onEvent(INIT).call(log, &ELog::start);
        onEvent(STOP).call(log, &ELog::stop);
        onEvent(TIMEOUT).call(log, &ELog::timeout);
    }
    /// unregisters from the Kernel and the FrameRegistry
    ~Experiment() {
        k.remove(ticker);
        if (statusTicker) k.remove(statusTicker);
        if (frames) {
            frames->removeHandler(1);
            frames->removeObserver();
        }
    }
    Experiment(const Experiment &) = delete;
    Experiment& operator=(const Experiment &) = delete;
    /// set frame registry from which Experiment will receive frames
    void registerWith(FrameRegistry &reg) {
        reg.setHandler(1, *this, &Experiment::handleFrame);
        reg.setObserver(*this, &Experiment::observe);
        frames = &reg;
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
    /// set Heartbeat timeout in ms, replaces an earlier notify Sink
    void setHeartbeatTimeout(uint32_t ms, Sink<Frame> &notify) {
        heartbeat.timeout = ms;
        heartbeat.reset(time);
        // notify other side if we timeout
        struct TMP :Schedule::Schedulable{
                Sink<Frame> &s;
                TMP(Sink<Frame>&s): s(s) {}
                void call() override{
                    auto f = Frame{1}.pack(false);
                    s.trypush(std::move(f));
                }};
        if (notifier) timeout.remove(notifier, k);
        notifier = timeout.call(new TMP{notify});
    }
    /// id of the status frame sent to pyWisp, see \ref enableStatus
    static constexpr uint8_t STATUS_ID = 63;
    /// bits of the project sent with the status, e.g. "motor driver ready"
    uint8_t statusFlags{};
    /** send a status frame every `ms` and at every start, also while IDLE
     *
     * pyWisp shows from it whether the rig responds and checks the start: the
     * frame carries a CRC32 over all frames received before the last start
     * command (id, length and payload of each, control frames excluded),
     * pyWisp compares it with what it sent. Parameters changed during the run
     * are followed by a sync command (control frame bits 0b110 and a sync id),
     * the rig answers it at once with the CRC32 of the frames since the last
     * control frame. Payload (little endian):
     *
     * | bytes | content                                          |
     * |-------|--------------------------------------------------|
     * | 0     | version, 1                                       |
     * | 1     | state, 0 IDLE, 1 RUN                             |
     * | 2     | \ref statusFlags                                 |
     * | 3     | start counter, incremented by every start command |
     * | 4-5   | number of frames received before that start      |
     * | 6-9   | their CRC32                                      |
     * | 10-13 | uptime of the rig [ms]                           |
     * | 14    | id of the last sync command                      |
     * | 15-16 | number of frames received before that sync       |
     * | 17-20 | their CRC32                                      |
     */
    void enableStatus(Sink<Frame> &out, uint32_t ms=250) {
        statusOut = &out;
        if (statusTicker) k.remove(statusTicker);
        statusTicker = k.every(ms, *this, &Experiment::statusTick);
        if (!statusOnInit) {
            onEvent(INIT).call(*this, &Experiment::sendStatus);
            statusOnInit = true;
        }
    }
    /// Logging facility
    ELog log{time, k.log};

private:
    State state_{};
    bool alive{};
    Schedule::Schedulable *notifier{}, *ticker{}, *statusTicker{};
    FrameRegistry *frames{};
    Sink<Frame> *statusOut{};
    bool statusOnInit{};
    /// frames received since the last start or stop command
    struct {
        CRC32 crc{};
        uint16_t count{};
    } received{};
    /// what had been received before the last start command (seq: counter)
    /// and before the last sync command (seq: id sent by pyWisp)
    struct Received {
        uint8_t seq{};
        uint16_t count{};
        uint32_t crc{};
    } started{}, synced{};
    void observe(Frame &f) {
        if (f.id == 1) return;
        received.crc.step(f.id);
        received.crc.step(f.b.len);
        for (size_t i = 0; i < f.b.len; ++i) received.crc.step(f.b.buf[i]);
        ++received.count;
    }
    void sendStatus(uint32_t) {
        if (!statusOut) return;
        auto f = Frame{STATUS_ID};
        f.pack<uint8_t>(1)
         .pack<uint8_t>(state == RUN ? 1 : 0)
         .pack<uint8_t>(statusFlags)
         .pack<uint8_t>(started.seq)
         .pack<uint16_t>(started.count)
         .pack<uint32_t>(started.crc)
         .pack<uint32_t>(k.time)
         .pack<uint8_t>(synced.seq)
         .pack<uint16_t>(synced.count)
         .pack<uint32_t>(synced.crc);
        statusOut->trypush(std::move(f));
    }
    void statusTick(uint32_t time, uint32_t) {
        sendStatus(time);
    }
    void statemachine() {
        State old = state;
        // very simple state machine
        state_ = alive ? RUN : IDLE;
        if (state != old) switch(old) { // handle state transitions == events
        case IDLE:
            time_ = 0;
            // deadline may still be set relative to the old time
            if (heartbeat) heartbeat.reset(time);
            k.schedule(time, init);
            idle.reset(); running.reset();
            break;
        case RUN:
            k.schedule(time, stop);
            break;
        }
        switch (state) { // handle states == recurring
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
            when = Deadline::after(now, timeout).when;
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
            // with heartbeat set: older rigs take a sync command for a heartbeat
            uint8_t sync:1;
            uint8_t _:5;
        } b = f.unpack<decltype(b)>();
        if (b.sync) {
            synced.seq = f.remaining() ? f.unpack<uint8_t>() : 0;
            synced.count = received.count;
            synced.crc = received.crc.finalize();
            received = {};
            (bool)heartbeat && heartbeat.reset(time);
            sendStatus(time);
        } else if (b.heartbeat) {
            (bool)heartbeat && heartbeat.reset(time);
        } else {
            if (b.alive) {
                started.seq++;
                started.count = received.count;
                started.crc = received.crc.finalize();
            }
            received = {};
            alive = b.alive;
        }
    }
} e;

