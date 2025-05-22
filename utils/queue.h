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
    enum : uint8_t {SPACE, Q} tag {SPACE};
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
    void destr() {
        switch (tag) {
        case SPACE: space.~Buffer(); return;
        case Q: q.~Buffer(); return;
        }
    }

public:
    ~Queue() { destr(); }
    /** allow copying */
    Queue(const Queue &other) : space(other.space.size * sizeof(T)), head{other.head.sz}, tail{other.tail.sz} {
        q.size = other.q.size;
        for (auto el : other.q) push(el);
    }
    Queue& operator=(const Queue &other) {
        if (this == &other) return *this;
        if (!space.size || space.size != other.space.size) {
            destr();
            space = other.space.size * sizeof(T);
            q.size = other.q.size;
            head = other.head.sz;
            tail = other.tail.sz;
        } else {
            while (!empty()) pop();
        }
        for (auto el : other.q) push(el);
        return *this;
    }
    /** allow moving */
    Queue(Queue &&other) noexcept
        : space(std::move(other.space)), head{other.head}, tail{other.tail} { }
    Queue& operator=(Queue &&other) noexcept {
        if (this == &other) return *this; // move to self
        destr();
        q = std::move(other.q);
        head = other.head;
        tail = other.tail;
        return *this;
    }
    /** create Queue directly from filled Buffer */
    Queue(const Buffer<T> &buf) : q(buf), tag{Q}, head{q.size}, tail{q.size} {}
    Queue(Buffer<T> &&buf) : q(std::move(buf)), tag{Q}, head{q.size}, tail{q.size} {}
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
