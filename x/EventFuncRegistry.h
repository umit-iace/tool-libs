#pragma once
#include "utils/Buffer.h"
#include "utils/Queue.h"
#include "x/Schedule.h"
struct EventFuncRegistry//: public Scheduler {
{
    using Func = void (*)(uint32_t time_ms);
    /* struct Callable { */
    /*     Func func; */
    /*     uint32_t sched_time; */
    /*     void call() { */
    /*         func(sched_time); */
    /*     } */
    /* }; */

    Buffer<Func> list{20};
    /* Queue<Callable, 20> q; */
    void reg(Func func) {
        list.append(func);
    }
    void schedule(Scheduler &s, uint32_t time) {
        for (Func &f : list) {
            s.push(new EventSchedulableFunc(f, {time}));
        }
    }
    /* void run() override { */
    /*     while (!q.empty()) { */
    /*         q.pop().call(); */
    /*     } */
    /* } */
};
