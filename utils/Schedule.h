#pragma once
#include "Queue.h"
template<int size>
struct TimeSchedule {
    using Func = void (*)(uint32_t, uint32_t);
    using Args = struct {
        uint32_t time_ms;
        uint32_t dt_ms;
    };
    struct Callable {
        Func func;
        Args args;
    };
    Queue<Callable, size> q;

    bool empty() {
        return q.empty();
    }
    void run() {
        auto c = q.pop();
        return c.func(c.args.time_ms, c.args.dt_ms);
    }
    void append(Func f, Args args) {
        q.push({.func=f, .args=args});
    }
};
