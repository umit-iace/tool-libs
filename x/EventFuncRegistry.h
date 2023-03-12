#pragma once
#include "x/Schedule.h"
namespace Schedule {
/** Oneshot function/method calling infrastructure
 *
 * the signature of a callable is
 * ```
 *      // will be called with absolute `time`
 *      void func_or_method (uint32_t time);
 * ```
 * when trying to schedule a call with a different signature
 * consider wrapping it in a lambda function
 * ```
 *      Schedule::Evented::Registry reg;
 *      reg.call( [](uint32_t t) {
 *              mynoargfunc();
 *          });
 * ```
 * alternatively you can create your own Schedulable on the fly
 * if capturing in a lambda doesn't work, see e.g. Experiment.h
 */
namespace Evented {
    /** oneshot Schedulable
     * keeps track of the scheduling time, as this will be passed
     * on to the wrapped call
     */
    struct Schedulable: public Schedule::Schedulable {
        uint32_t time{};
        bool schedule(uint32_t t) override {
            time = t;
            return true;
        }
    };
    /** oneshot callable function wrapper */
    struct Func: public Schedulable {
        using Call = void (*)(uint32_t);
        Call func{};
        Func(Call f): func(f) { }
        void call() override {
            return func(time);
        }
    };

    /** oneshot callable method wrapper */
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

/** registry of evented calls */
struct Registry : Schedule::Registry {
    /** register function to be scheduled for calling on Event */
    void call(Func::Call func) {
        list.append(new Func{func});
    }
    /** register method to be scheduled for calling on Event */
    template<typename T>
    void call(T& base, typename Method<T>::Call method) {
        list.append(new Method<T>{&base, method});
    }
    /** register plain Schedulable
     *
     * moves the ownership of the Schedulable to this registry
     */
    void call(Schedule::Schedulable *obj) {
        list.append(obj);
    }
};
}}
