#pragma once
#include "x/TimedFuncRegistry.h"

/** simple kernel for running tool-libs based applications */
extern class Kernel :
    public Schedule::Recurring::Registry,
    public Scheduler {
public:
    /** const access to global uptime */
    const uint32_t &time{time_};
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
