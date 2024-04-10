/** @file queue.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "buffer.h"

#include <core/streams.h>

/** simple Buffer backed queue implementation */
template <typename T>
class Queue : public Sink<T>, public Source<T> {
    union {
        Buffer<uint8_t> space;  // space
        Buffer<T> q;            // access
    };
    struct WrappingIndex {
        WrappingIndex(size_t sz) : sz(sz) { }
        size_t val{};
        const size_t sz{};
        operator size_t() {
            return val;
        }
        size_t operator++(int) {
            auto tmp = val;
            val = (val + 1) % sz;
            return tmp;
        }
    } head, tail;
public:
    ~Queue() { space.~Buffer(); }
    /** create Queue with constant size */
    Queue(size_t size=30) : space(size * sizeof(T)), head{size}, tail{size} {
        memset(space.buf, 0, size * sizeof(T));
        // now that we allocated enough space,
        // we can use the size & len for our own purposes
        q.size = size;
        q.len = 0;
    }
    using Sink<T>::push;
    /** move element into queue
     *
     * consider guarding with `if(! full()) ...`
     **/
    void push(T &&val) override {
        assert(q.len < q.size);
        q[tail++] = std::move(val);
        q.len++;
    }
    /** return reference to first element in queue */
    T& front() {
        assert(q.len != 0);
        return q[head];
    }
    /**
     * remove front of queue and return it
     *
     * does not check that there's something in the queue
     * guard with `if (size()) ...` or `if (! empty()) ...`
     */
    T pop() override {
        assert(q.len != 0);
        q.len--;
        return std::move(q[head++]);
    }
    /** return number of elements in queue */
    size_t size() {
        return q.len;
    }
    /** check if queue is empty */
    bool empty() override {
        return q.len == 0;
    }
    /** return true if queue is full */
    bool full() {
        return q.len == q.size;
    }
    /** return element at idx */
    T getAt(size_t idx) {
        return q[(head + idx) % q.size];
    }
};
