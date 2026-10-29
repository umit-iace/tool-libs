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
    struct KLog: Logger {
        KLog(const Time &t, Sink<Buffer<uint8_t>> &snk): Logger(snk), time(t) {}
        KLog(const Time &t): Logger(), time(t) {}
        const Time &time;
        virtual Buffer<uint8_t> pre() override {
            Buffer<uint8_t> ret = 256;
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
    /** call every ms */
    void tick(uint32_t dt_ms) {
        auto ok = schedule(time, *this);
        if (!ok) exit( 127 ); // too many items in scheduled queue. DYING.
        time_.fetch_add(dt_ms, std::memory_order_relaxed);
    }
    /** unregister and destroy a recurring call, e.g. before its object dies
     *
     * takes the handle returned by `every`. call from the main loop.
     */
    bool remove(Schedule::Schedulable *s) {
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
    Time time_{};
    bool go{true};
    int exit_code{};
    jmp_buf jbf{};
    friend void sighandler(int);
} k;
