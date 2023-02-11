#pragma once
#include "utils/Queue.h"
namespace Schedule {
struct Callable {
    virtual ~Callable(){}
    virtual bool schedule(uint32_t time) =0;
    virtual void call() =0;
};
struct Registry {
    Buffer<Callable *> list{20};
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
    Queue<Schedule::Callable*, 30> q;

    void schedule(uint32_t time, Schedule::Registry &reg) {
        for (auto &c: reg.list) {
            if (c->schedule(time)) q.push(c);
        }
    }
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
        }
    }
};
