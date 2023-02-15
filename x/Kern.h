#pragma once
#include "x/Schedule.h"
#include "x/TimedFuncRegistry.h"

inline struct Kernel {
    Scheduler s{};
    Schedule::Recurring::Registry always{};

    template<typename ...Args>
    constexpr void every(Args&& ...a) {
        return always.every(std::forward<Args>(a)...);
    }
    template<typename ...Args>
    constexpr void schedule(uint32_t t, Args&& ...a) {
        return s.schedule(t, std::forward<Args>(a)...);
    }
    void tick(uint32_t gtime) {
        s.schedule(gtime, always);
    }
    /** call this in the main loop to actually run the functions
     * previously scheduled in ``ms_tick``
     */
    void run() {
        s.run();
    }
    void idle();
} k;

