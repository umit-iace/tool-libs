#pragma once
#include "utils/Buffer.h"
#include "utils/Queue.h"
#include "x/Schedule.h"

struct TimedFuncRegistry {
    using Func = void (*)(uint32_t, uint32_t);
    inline uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }
    struct Container {
        Func func;
        uint32_t dt_ms, next;
    };

    Buffer<Container> list{20}; //< list of registered functions to be scheduled
    uint32_t next; //< next point in time when this registry shall be called

    /** register a function to be called every @dt_ms */
    void reg(Func func, uint32_t dt_ms) {
        list.append({.func = func, .dt_ms = dt_ms});
        next = 0;
    }
    void schedule(Scheduler &s, uint32_t time) {
        if (time < next) return;
        next = (uint32_t)-1;
        for (Container &c : list) {
            if (c.next == 0 || time+1 >= c.next) {
                c.next += c.dt_ms;
                next = min(next, c.next);
                // schedule the call of the registered function
                s.push(new TimeSchedulableFunc(c.func, {time, c.dt_ms}));
            }
        }
    }
    /** reset calling times of all registered functions */
    void reset() {
        next = 0;
        for (auto &c: list) {
            c.next = 0;
        }
    }
};
