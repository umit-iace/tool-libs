#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"
namespace Schedule { namespace Recurring {

using SFunc = void (*)(uint32_t, uint32_t);
template<typename T>
using SMethod = void (T::*)(uint32_t, uint32_t);

struct Schedulable: public Callable {
    uint32_t next{}, time{}, dt{};
    Schedulable(uint32_t dt) : dt{dt} { }
    bool schedule(uint32_t now) override {
        if (now < next) return false;
        next += dt;
        time = now;
        return true;
    }
    void reset() { next = 0; }
};

struct Func: public Schedulable {
    SFunc func;
    Func(SFunc f, uint32_t dt): Schedulable(dt), func(f) { }
    void call() override { return func(time, dt); }
};

template<typename T>
struct Method : public Schedulable {
    T* base;
    SMethod<T> method;
    Method(T* b, SMethod<T> m, uint32_t dt)
        : Schedulable(dt), base(b), method(m) { }
    void call() override {
        return (base->*method)(time, dt);
    }
};

struct Registry {
    Buffer<Callable *> list{20};
    operator Buffer<Callable *>&() { return list; }
    /** register a function to be called every @dt_ms */
    void every(uint32_t dt_ms, SFunc func) {
        list.append(new Func{func, dt_ms});
    }
    template<typename T>
    void every(uint32_t dt_ms, T& base, SMethod<T> method) {
        list.append(new Method<T>{&base, method, dt_ms});
    }
    /** reset calling times of all registered functions */
    void reset() {
        for (auto &c: list) {
            static_cast<Schedulable *>(c)->reset();
        }
    }
    ~Registry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
};
}}
