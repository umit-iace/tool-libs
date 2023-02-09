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
struct TimeSchedulableMethod : public SchedulableBase {
    using Base = T*;
    using Method = void (T::*)(uint32_t, uint32_t);
    using Args = struct {
        uint32_t time_ms;
        uint32_t dt_ms;
    };
    Base base;
    Method method;
    Args args;
    TimeSchedulableMethod(Base b, Method m, Args a): base(b), method(m), args(a) { }
    virtual void call() override {
        return (base->*method)(args.time_ms, args.dt_ms);
    }
};

struct Scheduler {
    Queue<SchedulableBase*, 30> q;
    void push(SchedulableBase *f) {
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
