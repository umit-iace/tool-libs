/** @file timed.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "schedule.h"
namespace Schedule {
/** Recurring function/method calling infrastructure
 *
 * the signature of a recurringly callable is
 * ```
 *      // will be called with absolute `time`, and
 *      // the period `dt` with which it has been scheduled
 *      void func_or_method (uint32_t time, uint32_t dt);
 * ```
 * when trying to schedule a call with a different signature
 * consider wrapping it in a lambda function
 * ```
 *      Schedule::Recurring::Registry reg;
 *      reg.every( 10, //ms
 *          [](uint32_t t, uint32_t dt) {
 *              mynoargfunc();
 *          });
 * ```
 **/
namespace Recurring {
    /** recurring Schedulable
     * keeps track of time in order to make recurring scheduling work
     */
    struct Schedulable: public Schedule::Schedulable {
        uint32_t next{}, time{}, dt{};
        bool armed{};
        Schedulable(uint32_t dt) : dt{dt} { }
        /** only schedule if dt ms passed since last run
         *
         * wrap-around safe for periods below ~24.8 days
         */
        bool schedule(uint32_t now) override {
            if (!armed) {
                // first call right away, then aligned to multiples of dt
                // if registered before dt passed
                armed = true;
                next = now < dt ? dt : now + dt;
            } else {
                if ((int32_t)(now - next) < 0) return false;
                next += dt;
                // don't try to catch up on missed calls, e.g. after
                // registering late or after time jumped
                if ((int32_t)(next - now) <= 0) next = now + dt;
            }
            time = now;
            return true;
        }
        void reset() { armed = false; }
    };
    /** recurringly callable function wrapper */
    struct Func: public Schedulable {
        /** signature of recurringly callable function */
        using Call = void (*)(uint32_t, uint32_t);
        Call func;
        Func(Call f, uint32_t dt): Schedulable(dt), func(f) { }
        void call() override { return func(time, dt); }
    };

    /** recurringly callable method wrapper */
    template<typename T>
    struct Method : public Schedulable {
        /** signature of recurringly callable method */
        using Call = void (T::*)(uint32_t, uint32_t);
        T* base;
        Call method;
        Method(T* b, Call m, uint32_t dt)
            : Schedulable(dt), base(b), method(m) { }
        void call() override {
            return (base->*method)(time, dt);
        }
    };
/** registry of recurring calls */
struct Registry : Schedule::Registry {

    /** register a function to be called every dt_ms
     *
     * @return handle for `remove`, nullptr if dt_ms is 0
     */
    Schedulable *every(uint32_t dt_ms, typename Func::Call func) {
        if (!dt_ms) return nullptr;
        auto s = new Func{func, dt_ms};
        add(s);
        return s;
    }
    /** register a method to be called every dt_ms
     *
     * @return handle for `remove`, nullptr if dt_ms is 0
     */
    template<typename T>
    Schedulable *every(uint32_t dt_ms, T& base,
            typename Method<T>::Call method) {
        if (!dt_ms) return nullptr;
        auto s = new Method<T>{&base, method, dt_ms};
        add(s);
        return s;
    }
    /** reset calling times of all registered functions
     *
     * this may be necessary when 'global time' jumps backwards
     * e.g. when 'global time' is actually 'experiment time' and
     * the experiment is restarted
     */
    void reset() {
        for (auto &c: list) {
            static_cast<Schedulable *>(c)->reset();
        }
    }
};
}}
