#pragma once
#include "utils/Queue.h"
struct SchedulableBase {
    virtual ~SchedulableBase(){}
    virtual void schedule(Queue<SchedulableBase*,30> &s, uint32_t time) =0;
    virtual void call() =0;
    virtual void reset() {}
};

struct TimeSchedulableFunc: public SchedulableBase {
    using Func = void (*)(uint32_t, uint32_t);
    using Args = struct {
        uint32_t time_ms;
        uint32_t dt_ms;
    };
    Func func;
    Args args{};
    uint32_t next{};
    TimeSchedulableFunc(){}
    TimeSchedulableFunc(Func f, Args a): func(f), args(a) { }
    void call() override {
        return func(args.time_ms, args.dt_ms);
    }
    void schedule(Queue<SchedulableBase*, 30> &q, uint32_t time) override {
        if (time < next) return;
        if (next == 0 || time+1 >= next) {
            next += args.dt_ms;
            args.time_ms = time;
            // schedule the call of the registered function
            q.push(this);
        }
    }
    void reset() override { next = 0; }
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
    uint32_t next{};
    TimeSchedulableMethod(){}
    TimeSchedulableMethod(Base b, Method m, Args a): base(b), method(m), args(a) { }
    void call() override {
        return (base->*method)(args.time_ms, args.dt_ms);
    }
    void schedule(Queue<SchedulableBase*,30> &q, uint32_t time) {
        if (time < next) return;
        if (next == 0 || time+1 >= next) {
            next += args.dt_ms;
            args.time_ms = time;
            // schedule the call of the registered function
            q.push(this);
        }
    }
    void reset() override { next = 0; }
};
struct EventSchedulableFunc: public SchedulableBase {
    using Func = void (*)(uint32_t);
    using Args = struct {
        uint32_t time_ms;
    };
    Func func{};
    Args args{};
    EventSchedulableFunc(){}
    EventSchedulableFunc(Func f, Args a): func(f), args(a) { }
    void call() override {
        return func(args.time_ms);
    }
    void schedule(Queue<SchedulableBase*,30> &q, uint32_t time) override {
        args.time_ms = time;
        q.push(this);
    }
};
template<typename T>
struct EventSchedulableMethod: public SchedulableBase {
    using Base = T*;
    using Method = void (T::*)(uint32_t);
    using Args = struct {
        uint32_t time_ms;
    };
    Base base;
    Method method;
    Args args;
    EventSchedulableMethod(){}
    EventSchedulableMethod(Base b, Method m, Args a): base(b), method(m), args(a) { }
    void call() override {
        return (base->*method)(args.time_ms);
    }
    void schedule(Queue<SchedulableBase*,30> &q, uint32_t time) override {
        args.time_ms = time;
        q.push(this);
    }
};

// round robin, because we're not smart enough for anything else
struct Scheduler {
    Queue<SchedulableBase*, 30> q;
    void push(SchedulableBase *f) {
        q.push(f);
    }
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
        }
    }
};
