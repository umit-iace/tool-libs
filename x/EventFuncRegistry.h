#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"
namespace Schedule { namespace Evented {
struct Registry : Schedule::Registry {

    struct Schedulable: public Schedule::Schedulable {
        uint32_t time{};
        bool schedule(uint32_t t) override {
            time = t;
            return true;
        }
    };

    struct Func: public Schedulable {
        using Call = void (*)(uint32_t);
        Call func{};
        Func(Call f): func(f) { }
        void call() override {
            return func(time);
        }
    };

    template<typename T>
    struct Method: public Schedulable {
        using Call = void (T::*)(uint32_t);
        T* base;
        Call method;
        Method(T* b, Call m): base(b), method(m) { }
        void call() override {
            return (base->*method)(time);
        }
    };

    /** register Function to be scheduled for calling on Event */
    void call(Func::Call func) {
        list.append(new Func{func});
    }
    template<typename T>
    void call(T& base, typename Method<T>::Call method) {
        list.append(new Method<T>{&base, method});
    }
};
}}
