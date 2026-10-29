/** @file schedule.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "utils/queue.h"
#include "critical.h"
struct Scheduler;
/** namespace wrapping all functionality to do with scheduling */
namespace Schedule {
//XXX: figure out a way to simplify this whole namespace
//I'm not satisfied with each schedulable deciding whether it wants to
//run by itself. That probably belongs into the registries instead.
//Then we could probably do away with the whole 'Schedulable' concept
//and just keep 'callable's or smth
/** represents an object that can be scheduled for later calling
 *
 * is the poor man's closure
 * this can be implemented to enable different behaviours. See the
 * Recurring & Evented namespaces
 */
struct Schedulable {
    virtual ~Schedulable(){}
    /** return true if object should be scheduled at this time */
    virtual bool schedule(uint32_t) { return true; }
    /** this method is called to run the Schedulable */
    virtual void call()=0;
};
/** a registry of Schedulable which can be given to the Scheduler for
 * actual scheduling of calls
 *
 * this is implemented / extended for the actual registration of the
 * specific schedulables. see Evented and Recurring
 *
 * useful to have a bunch of these for the different situations
 * where scheduling is necessary. see Experiment for inspiration
 */
struct Registry {
    Buffer<Schedulable *> list = 20;
    /** append Schedulable, growing the list if necessary */
    void add(Schedulable *s) {
        // the Kernel might be iterating the list from an interrupt:
        // allocate outside, publish under the CriticalSection, which also
        // keeps the compiler from storing len before the entry (or before
        // *s is fully constructed)
        Schedulable **bigger = nullptr, **old = nullptr;
        size_t sz = 0;
        if (list.len == list.size) {
            sz = list.size ? 2 * list.size : 20;
            bigger = new Schedulable*[sz];
            for (size_t i = 0; i < list.len; ++i) bigger[i] = list.buf[i];
        }
        {
            CriticalSection cs;
            if (bigger) {
                old = list.buf;
                list.buf = bigger;
                list.size = sz;
            }
            list.append(s);
        }
        delete[] old;
    }
    /** take Schedulable out of the list, ownership goes to the caller
     *
     * it may still be queued in a Scheduler, see remove(s, scheduler)
     * @return false if s isn't in this registry
     */
    bool remove(Schedulable *s) {
        CriticalSection cs;
        for (size_t i = 0; i < list.len; ++i) {
            if (list.buf[i] != s) continue;
            for (; i + 1 < list.len; ++i) list.buf[i] = list.buf[i + 1];
            --list.len;
            return true;
        }
        return false;
    }
    /** unregister and destroy Schedulable, e.g. before the object it calls
     * dies
     *
     * also removes pending calls from the Scheduler this registry is
     * scheduled with. call from the main loop only, the Schedulable may
     * remove itself from within its call.
     */
    inline bool remove(Schedulable *s, Scheduler &sched);
    Registry() = default;
    // owns the Schedulables
    Registry(const Registry &) = delete;
    Registry& operator=(const Registry &) = delete;
    ~Registry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
};
}

/// round robin, because we're not smart enough for anything else
///
/// safe to schedule from interrupts (e.g. Kernel::tick in SysTick)
/// while running in the main loop
struct Scheduler {
    // this is pointing to the actual callables in the registries
    // we do not own them
    Queue<Schedule::Schedulable*> q;
    /** schedule all registered schedulables of registry if necessary */
    bool schedule(uint32_t time, Schedule::Registry &reg) {
        CriticalSection cs;
        for (auto &c: reg.list) {
            if (q.full()) return false;
            if (c->schedule(time)) q.push(c);
        }
        return true;
    }
    /** drop pending calls of s */
    void forget(Schedule::Schedulable *s) {
        CriticalSection cs;
        for (size_t n = q.size(); n; --n) {
            auto c = q.pop();
            if (c != s) q.push(std::move(c));
        }
    }
    /** run scheduled calls */
    void run() {
        while (true) {
            Schedule::Schedulable *s;
            {
                CriticalSection cs;
                if (q.empty()) return;
                s = q.pop();
            }
            s->call();
        }
    }
};

inline bool Schedule::Registry::remove(Schedulable *s, Scheduler &sched) {
    if (!remove(s)) return false;
    sched.forget(s);
    delete s;
    return true;
}
