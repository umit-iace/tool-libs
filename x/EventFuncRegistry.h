#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"
namespace Schedule { namespace Evented {

struct Schedulable: public Callable {
    uint32_t time{};
    bool schedule(uint32_t t) override {
        time = t;
        return true;
    }
};
using SFunc = void (*)(uint32_t);
template<typename T>
using SMethod = void (T::*)(uint32_t);

struct Func: public Schedulable {
    SFunc func{};
    Func(SFunc f): func(f) { }
    void call() override {
        return func(time);
    }
};

template<typename T>
struct Method: public Schedulable {
    T* base;
    SMethod<T> method;
    Method(T* b, SMethod<T> m): base(b), method(m) { }
    void call() override {
        return (base->*method)(time);
    }
};

struct Registry {
    Buffer<Schedulable *> list{20};
    /** register Function to be scheduled for calling on Event */
    void onEvent(SFunc func) {
        list.append(new Func{func});
    }
    template<typename T>
    void onEvent(T& base, SMethod<T> method) {
        list.append(new Method<T>{&base, method});
    }
    void schedule(Scheduler &s, uint32_t time) {
        for (auto &f : list) {
            if (f->schedule(time)) s.push(f);
        }
    }
    ~Registry() {
        for (auto &entry : list) {
            delete entry;
        }
    }
};
}}
