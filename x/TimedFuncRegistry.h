#pragma once
#include "utils/Buffer.h"
#include "utils/Queue.h"
#include "x/Schedule.h"
#define min(a,b) ((a)<(b)?(a):(b))
struct TimedFuncRegistry//: public Scheduler {
{
    using Func = void (*)(uint32_t, uint32_t);
    struct Container {
        Func func;
        uint32_t dt_ms, next;
    };
    /* struct Callable { */
    /*     Func func; */
    /*     uint32_t sched_time; */
    /*     uint32_t dt_ms; */
    /*     void call() { */
    /*         func(sched_time, dt_ms); */
    /*     } */
    /* }; */

    Buffer<Container> list{20};
    uint32_t next;
    /* Queue<Callable, 20> q; */

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
            /* if (c.last == 0 || time+1 >= c.last + c.dt_ms) { */
            /*     c.last = time+1; */
                // schedule the call of the registered function
                s.push(new TimeSchedulableFunc(c.func, {time, c.dt_ms}));
            }
        }
    }
    void reset() {
        next = 0;
        for (auto &c: list) {
            c.next = 0;
        }
    }
    /* void run() override { */
    /*     while (!q.empty()) { */
    /*         q.pop().call(); */
    /*     } */
    /* } */
};
#undef min
