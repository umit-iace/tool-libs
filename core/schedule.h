/** @file schedule.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "utils/queue.h"
#include "critical.h"
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
        if (list.len == list.size) {
            // fully build the bigger list before swapping it in,
            // the Kernel might be iterating the old one from an interrupt
            size_t sz = list.size ? 2 * list.size : 20;
            Schedulable **bigger = new Schedulable*[sz];
            for (size_t i = 0; i < list.len; ++i) bigger[i] = list.buf[i];
            Schedulable **old = list.buf;
            list.buf = bigger;
            list.size = sz;
            delete[] old;
        }
        list.append(s);
    }
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
