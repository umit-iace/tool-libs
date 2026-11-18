/** @file deferred.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <utils/queue.h>
#include <core/kern.h>
#include <core/critical.h>

/** completed requests, handed from the interrupt to the main loop
 *
 * the device callbacks run in the main loop: they may allocate, log or
 * push new requests. the interrupt only moves the request (no heap) and
 * posts this to the kernel's scheduler.
 */
template<typename Request>
struct Deferred : Schedule::Schedulable {
    Queue<Request> done;
    /** requests completed while `done` was full, their callback is lost */
    uint32_t lost{};

    Deferred() {
        // in case posting failed (scheduler queue full)
        fallback = k.every(10, *this, &Deferred::poll);
    }
    ~Deferred() {
        k.remove(fallback);
        k.forget(this);
    }
    Deferred(const Deferred &) = delete;
    Deferred& operator=(const Deferred &) = delete;

    /** in the interrupt: queue the completed request */
    void complete(Request &&rq) {
        if (done.full()) {
            ++lost;
            return;
        }
        done.push(std::move(rq));
        if (!posted) posted = k.post(this);
    }
    /** main loop: hand the completed requests to their devices */
    void call() override {
        while (true) {
            Request rq{};
            {
                CriticalSection cs;
                if (done.empty()) {
                    posted = false;
                    return;
                }
                rq = done.pop();
            }
            rq.dev->callback(rq);
        }
    }
private:
    bool posted{};
    Schedule::Schedulable *fallback{};
    void poll(uint32_t, uint32_t) { call(); }
};
