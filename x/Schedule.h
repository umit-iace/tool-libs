#pragma once
#include "utils/Queue.h"
namespace Schedule {
struct Callable {
    virtual ~Callable(){}
    virtual bool schedule(uint32_t time) =0;
    virtual void call() =0;
};
}

/// round robin, because we're not smart enough for anything else
struct Scheduler {
    // this is pointing to the actual callables in the registries
    Queue<Schedule::Callable*, 30> q;
    void push(Schedule::Callable *f) {
        q.push(f);
    }
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
        }
    }
};
