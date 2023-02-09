#pragma once
#include "utils/Queue.h"
struct SchedulableBase {
    virtual ~SchedulableBase(){}
    virtual void call() =0;
};

struct TimeSchedulableFunc: public SchedulableBase {
    using Func = void (*)(uint32_t, uint32_t);
    using Args = struct {
        uint32_t time_ms;
        uint32_t dt_ms;
    };
    Func func;
    Args args;
    TimeSchedulableFunc(Func f, Args a): func(f), args(a) { }
    virtual void call() override {
        return func(args.time_ms, args.dt_ms);
    }
};
struct EventSchedulableFunc: public SchedulableBase {
    using Func = void (*)(uint32_t);
    using Args = struct {
        uint32_t time_ms;
    };
    Func func;
    Args args;
    EventSchedulableFunc(Func f, Args a): func(f), args(a) { }
    virtual void call() override {
        return func(args.time_ms);
    }
};

template<typename T>
struct SchedulableFunc : public SchedulableBase {

};

struct Scheduler {
    Queue<SchedulableBase*, 30> q;
    void push(EventSchedulableFunc *f) {
        q.push(f);
    }
    void push(TimeSchedulableFunc *f) {
        q.push(f);
    }
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
            delete s;
        }
    }
};

struct TimeCallable {
    using Func = void (*)(uint32_t, uint32_t);
    using Args = struct {
        uint32_t time_ms;
        uint32_t dt_ms;
    };
    Func func;
    Args args;
    void operator()() {
        return func(args.time_ms, args.dt_ms);
    }
};
struct EventCallable {
    using Func = void (*)(uint32_t);
    using Args = struct {
        uint32_t time_ms;
    };
    Func func;
    Args args;
    void operator()() {
        return func(args.time_ms);
    }
};

using TimeSchedule = Queue<TimeCallable, 20>;
using EventSchedule = Queue<EventCallable, 20>;
