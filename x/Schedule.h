#pragma once
#include "utils/Queue.h"
struct SchedulableBase {
    virtual ~SchedulableBase(){}
    virtual bool schedule(uint32_t time) =0;
    virtual void call() =0;
};

// round robin, because we're not smart enough for anything else
struct Scheduler {
    Queue<SchedulableBase*, 30> q;
    void push(SchedulableBase *f) {
        q.push(f);
    }
    void run() {
        while (!q.empty()) {
            auto s = q.pop();
            s->call();
        }
    }
};
