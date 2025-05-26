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
    Buffer<T> q;
    struct WrappingIndex {
        WrappingIndex(size_t sz) : sz(sz) { }
        WrappingIndex(const WrappingIndex &other) : val(other.val), sz(other.sz) {}
        WrappingIndex& operator=(const WrappingIndex &other) {
            val = other.val;
            sz = other.sz;
            return *this;
        }
        size_t val{};
        size_t sz{};
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
    /** create Queue directly from filled Buffer */
    Queue(const Buffer<T> &buf) : q(buf), head{q.size}, tail{q.size} {}
    Queue(Buffer<T> &&buf) : q(std::move(buf)), head{q.size}, tail{q.size} {}
    /** create Queue with constant size */
    Queue(size_t size=30)
        : q(size)
        , head{size}
        , tail{size}
    { }
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
    /**
     * shift head of queue without touching underlying memory
     */
    void drop() {
        assert(q.len != 0);
        q.len--;
        head++;
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
    bool full() override {
        return q.len == q.size;
    }
    /** return element at idx */
    T getAt(size_t idx) {
        return q[(head + idx) % head.sz];
    }
};
