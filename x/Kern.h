#pragma once
#include "x/Schedule.h"
#include "x/TimedFuncRegistry.h"

/** simple kernel for running tool-libs based applications */
extern struct Kernel {
    Scheduler s{};
    Schedule::Recurring::Registry always{};

    /** register recurring callback */
    template<typename ...Args>
    constexpr void every(Args&& ...a) {
        return always.every(std::forward<Args>(a)...);
    }
    /** pass registry to scheduler for actual scheduling */
    void schedule(uint32_t time, Schedule::Registry &reg) {
        return s.schedule(time, reg);
    }
    /** call every ms */
    void tick(uint32_t gtime) {
        s.schedule(gtime, always);
    }
    /** kernel entry point */
    [[noreturn]] void run () {
        while(true) {
            s.run();
            idle();
        }
    }
    /** implement to not burn unnecessary cpu cycles
     * a simple sleep or 'wait for interrupt' will do
     */
    void idle();
} k;
