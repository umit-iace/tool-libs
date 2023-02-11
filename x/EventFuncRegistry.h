#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"

struct EventSchedulable: public SchedulableBase {
    uint32_t time{};
    bool schedule(uint32_t t) override {
        time = t;
        return true;
    }
};

struct EventSchedulableFunc: public EventSchedulable {
    using Func = void (*)(uint32_t);
    Func func{};
    EventSchedulableFunc(Func f): func(f) { }
    void call() override {
        return func(time);
    }
};
template<typename T>
struct EventSchedulableMethod: public EventSchedulable {
    using Base = T*;
    using Method = void (T::*)(uint32_t);
    Base base;
    Method method;
    EventSchedulableMethod(Base b, Method m): base(b), method(m) { }
    void call() override {
        return (base->*method)(time);
    }
};

struct EventFuncRegistry {
    using Func = void (*)(uint32_t);
    template<typename T>
    using Method = void (T::*)(uint32_t);
    Buffer<EventSchedulable *> list{20};
    ~EventFuncRegistry() {
        for (auto &entry : list) {
            delete entry;
        }
    }
    /** register Function to be scheduled for calling on Event */
    void onEvent(Func func) {
        list.append(new EventSchedulableFunc{func});
    }
    template<typename T>
    void onEvent(T& base, Method<T> method) {
        list.append(new EventSchedulableMethod<T>{&base, method});
    }
    void schedule(Scheduler &s, uint32_t time) {
        for (auto &f : list) {
            if (f->schedule(time)) s.push(f);
        }
    }
};
