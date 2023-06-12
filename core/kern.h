/** @file kern.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "timed.h"
#include "logger.h"

/** simple kernel for running tool-libs based applications
 *
 * also provides logging functionality
 */
extern class Kernel :
    public Schedule::Recurring::Registry,
    public Scheduler {
    struct KLog: Logger {
        KLog(const uint32_t &t, Sink<Buffer<uint8_t>> &snk): Logger(snk), time(t) {}
        KLog(const uint32_t &t): Logger(), time(t) {}
        const uint32_t &time;
        virtual Buffer<uint8_t> pre() override {
            Buffer<uint8_t> ret = 256;
            ret.len = snprintf((char*)ret.buf, ret.size, "(@%ldms): ", time);
            return ret;
        }
    };
public:
    /** const access to global uptime */
    const uint32_t &time{time_};
    KLog log{time};
    /** point logger to new sink */
    void initLog(Sink<Buffer<uint8_t>> &snk) {
        new(&log) KLog{time, snk};
    }
    /** call every ms */
    void tick(uint32_t dt_ms) {
        schedule(time, *this);
        time_ += dt_ms;
    }
    /** kernel entry point */
    [[noreturn]] void run () {
        while(true) {
            Scheduler::run();
            idle();
        }
    }
    /** implement to not burn unnecessary cpu cycles
     *
     * a simple sleep or 'wait for interrupt' will do
     *
     * if running in a single thread, you probably want to
     * call `tick()` in this method to advance the time
     */
    void idle();
private:
    uint32_t time_{};
} k;
