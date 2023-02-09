#pragma once
#include "utils/Buffer.h"
#include "utils/Queue.h"
#include "x/Schedule.h"

struct TimedFuncRegistry {
    using Func = void (*)(uint32_t, uint32_t);
    template <typename T>
    using Method = void (T::*)(uint32_t, uint32_t);
    ~TimedFuncRegistry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
    Buffer<SchedulableBase *> list{20}; //< list of registered functions to be scheduled

    /** register a function to be called every @dt_ms */
    void every(uint32_t dt_ms, Func func) {
        list.append(new TimeSchedulableFunc{func, {.dt_ms = dt_ms}});
    }
    template<typename T>
    void every(uint32_t dt_ms, T& base, Method<T> method) {
        list.append(new TimeSchedulableMethod<T>(&base, method, {.dt_ms=dt_ms}));
    }

    void schedule(Scheduler &s, uint32_t time) {
        for (auto &c : list) {
            c->schedule(s.q, time);
        }
    }
    /** reset calling times of all registered functions */
    void reset() {
        for (auto &c: list) {
            c->reset();
        }
    }
};
