#pragma once
#include "utils/Buffer.h"
#include "utils/Queue.h"
#include "x/Schedule.h"

struct TimeSchedulable: public SchedulableBase {
    uint32_t next{}, time{}, dt{};
    TimeSchedulable(uint32_t dt) : dt{dt} { }
    bool schedule(uint32_t now) override {
        if (now < next) return false;
        next += dt;
        time = now;
        return true;
    }
    void reset() { next = 0; }
};

struct TSFunc: public TimeSchedulable {
    using Func = void (*)(uint32_t, uint32_t);
    Func func;
    TSFunc(Func f, uint32_t dt): TimeSchedulable(dt), func(f) { }
    void call() override { return func(time, dt); }
};

template<typename T>
struct TSMethod : public TimeSchedulable {
    using Base = T*;
    using Method = void (T::*)(uint32_t, uint32_t);
    Base base;
    Method method;
    TSMethod(Base b, Method m, uint32_t dt)
        : TimeSchedulable(dt), base(b), method(m) { }
    void call() override {
        return (base->*method)(time, dt);
    }
};

struct TimedFuncRegistry {
    using Func = void (*)(uint32_t, uint32_t);
    template <typename T>
    using Method = void (T::*)(uint32_t, uint32_t);
    ~TimedFuncRegistry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
    Buffer<TimeSchedulable *> list{20}; //< list of registered functions to be scheduled

    /** register a function to be called every @dt_ms */
    void every(uint32_t dt_ms, Func func) {
        list.append(new TSFunc{func, dt_ms});
    }
    template<typename T>
    void every(uint32_t dt_ms, T& base, Method<T> method) {
        list.append(new TSMethod<T>{&base, method, dt_ms});
    }

    void schedule(Scheduler &s, uint32_t time) {
        for (auto &c : list) {
            if (c->schedule(time)) s.push(c);
        }
    }
    /** reset calling times of all registered functions */
    void reset() {
        for (auto &c: list) {
            c->reset();
        }
    }
};
