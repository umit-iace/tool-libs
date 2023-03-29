/** @file schedule.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "utils/queue.h"
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
    virtual bool schedule(uint32_t now) { return true; }
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
    Buffer<Schedulable *> list{20};
    ~Registry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
};
}

/// round robin, because we're not smart enough for anything else
struct Scheduler {
    // this is pointing to the actual callables in the registries
    // we do not own them
    Queue<Schedule::Schedulable*> q;
    /** schedule all registered schedulables of registry if necessary */
    void schedule(uint32_t time, Schedule::Registry &reg) {
        for (auto &c: reg.list) {
            if (c->schedule(time)) q.push(c);
        }
    }
    /** run scheduled calls */
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
        }
    }
};
