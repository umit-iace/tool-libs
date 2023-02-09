#pragma once
#include "utils/Buffer.h"
#include "x/Schedule.h"

struct EventFuncRegistry {
    using Func = void (*)(uint32_t);
    template<typename T>
    using Method = void (T::*)(uint32_t);
    Buffer<SchedulableBase *> list{20};
    ~EventFuncRegistry() {
        for (auto &entry : list) {
            delete entry;
        }
    }
    /** register Function to be scheduled for calling on Event */
    void onEvent(Func func) {
        list.append(new EventSchedulableFunc{func, {0}});
    }
    template<typename T>
    void onEvent(T& base, Method<T> method) {
        list.append(new EventSchedulableMethod<T>(&base, method, {0}));
    }
    void schedule(Scheduler &s, uint32_t time) {
        for (auto &f : list) {
            f->schedule(s.q, time);
        }
            /* s.push(new EventSchedulableFunc(f, {time})); */
        /* } */
    }
};
