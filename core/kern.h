/** @file kern.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "timed.h"
#include "logger.h"
#include <atomic>
#include <setjmp.h>
#include <inttypes.h>

/** simple kernel for running tool-libs based applications
 *
 * also provides logging functionality
 */
extern class Kernel :
    public Schedule::Recurring::Registry,
    public Scheduler {
    using Time = std::atomic<uint32_t>;
    using Schedulable = Schedule::Recurring::Schedulable;
    struct KLog: Logger {
        KLog(const Time &t, Sink<Buffer<uint8_t>> &snk): Logger(snk), time(t) {}
        KLog(const Time &t): Logger(), time(t) {}
        const Time &time;
        virtual Buffer<uint8_t> pre() override {
            Buffer<uint8_t> ret(256);
            ret.len = written(snprintf((char*)ret.buf, ret.size, "(@%" PRIu32 "ms): ",
                        time.load(std::memory_order_relaxed)), ret.size);
            return ret;
        }
    };
public:
    /** const access to global uptime [ms]
     *
     * advanced from the tick interrupt, atomic so reading it in loops
     * always sees the current value
     */
    const Time &time{time_};
    KLog log{time};
    /** point logger to new sink */
    void initLog(Sink<Buffer<uint8_t>> &snk) {
        new(&log) KLog{time, snk};
    }
    Kernel() {
        // calls registered by constructors of globals that ran before ours
        constructed = true;
        for (size_t i = 0; i < nearly; ++i) add(early[i]);
        nearly = 0;
    }
    /** register a function to be called every dt_ms
     *
     * can be used before the Kernel itself is constructed, e.g. from the
     * constructors of other global objects
     * @return handle for `remove`, nullptr if dt_ms is 0
     */
    static Schedulable *every(uint32_t dt_ms, Schedule::Recurring::Func::Call func) {
        if (!dt_ms) return nullptr;
        return adopt(new Schedule::Recurring::Func{func, dt_ms});
    }
    /** register a method to be called every dt_ms, see above */
    template<typename T>
    static Schedulable *every(uint32_t dt_ms, T& base,
            typename Schedule::Recurring::Method<T>::Call method) {
        if (!dt_ms) return nullptr;
        return adopt(new Schedule::Recurring::Method<T>{&base, method, dt_ms});
    }
    /** call every ms */
    void tick(uint32_t dt_ms) {
        // a full queue is counted in overruns, it grows in the main loop
        schedule(time, *this);
        time_.fetch_add(dt_ms, std::memory_order_relaxed);
    }
    /** unregister and destroy a recurring call, e.g. before its object dies
     *
     * takes the handle returned by `every`. call from the main loop.
     */
    bool remove(Schedule::Schedulable *s) {
        if (!constructed) {
            for (size_t i = 0; i < nearly; ++i) {
                if (early[i] != s) continue;
                early[i] = early[--nearly];
                delete s;
                return true;
            }
            return false;
        }
        return Registry::remove(s, *this);
    }
    /** kernel entry point */
    int run () {
        setjmp(jbf);
        while(go) {
            Scheduler::run();
            idle();
        }
        return exit_code;
    }
    /** stop kernel */
    void exit(int code=0) {
        go = false;
        exit_code = code;
    }
    /** implement to not burn unnecessary cpu cycles
     *
     * a simple sleep or 'wait for interrupt' will do
     *
     * if running in a single thread, you probably want to
     * call `tick()` in this method to advance the time
     */
    void idle();

    /** implement in simulation backend to be able to speed up time.
     *
     * takes step size in microseconds. default is 1000
     */
    void setTimeStep(uint16_t dt_us);
private:
    /** registrations before the constructor ran, constant initialized */
    static constexpr size_t EARLY = 32;
    static inline Schedule::Schedulable *early[EARLY]{};
    static inline size_t nearly{};
    static inline bool constructed{};
    static Schedulable *adopt(Schedulable *s);
    Time time_{};
    bool go{true};
    int exit_code{};
    jmp_buf jbf{};
    friend void sighandler(int);
} k;

inline Kernel::Schedulable *Kernel::adopt(Schedulable *s) {
    if (constructed) {
        k.add(s);
    } else {
        // too many globals registering before the Kernel exists
        assert(nearly < EARLY);
        if (nearly >= EARLY) {
            delete s;
            return nullptr;
        }
        early[nearly++] = s;
    }
    return s;
}
