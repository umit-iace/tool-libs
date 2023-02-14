#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"
namespace Schedule { namespace Recurring {
struct Registry : Schedule::Registry {

    struct Schedulable: public Schedule::Schedulable {
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
        using Call = void (*)(uint32_t, uint32_t);
        Call func;
        Func(Call f, uint32_t dt): Schedulable(dt), func(f) { }
        void call() override { return func(time, dt); }
    };

    template<typename T>
    struct Method : public Schedulable {
        using Call = void (T::*)(uint32_t, uint32_t);
        T* base;
        Call method;
        Method(T* b, Call m, uint32_t dt)
            : Schedulable(dt), base(b), method(m) { }
        void call() override {
            return (base->*method)(time, dt);
        }
    };

    /** register a function to be called every @dt_ms */
    void every(uint32_t dt_ms, Func::Call func) {
        list.append(new Func{func, dt_ms});
    }
    template<typename T>
    void every(uint32_t dt_ms, T& base,
            typename Method<T>::Call method) {
        list.append(new Method<T>{&base, method, dt_ms});
    }
    /** reset calling times of all registered functions */
    void reset() {
        for (auto &c: list) {
            static_cast<Schedulable *>(c)->reset();
        }
    }
};
}}
