/** @file Queue.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "Interfaces.h"

/**
 * simple Array backed circle buffer queue to be used as FIFO
 */
template <typename T, int sz>
class Queue : public Push<T>, public Pull<T> {
    uint8_t space[sz * sizeof(T)]{};
    struct QueueAccess {
        T *ptr;
        T& operator[](size_t i) {
            assert(i < sz);
            return ptr[i];
        }
    } q {(T*)space}; // Typed pointer into empty uint8_t[] space
    size_t len{};
    struct ArrayIndex {
        size_t val{};
        operator size_t() {
            return val;
        }
        size_t operator++() {
            val = (val + 1) % sz;
            return val;
        }
    } head, tail;
public:
    /**
     * move element into queue
     */
    void push(T &&val) override {
        log("moving ..\n");
        assert(len < sz);
        if (len == 0) tail = {0};
        else ++tail;
        q[tail] = std::move(val);
        ++len;
    }
    /**
     * return reference to first element in queue
     */
    T& front() {
        assert(len != 0);
        return q[head];
    }
    /**
     * return reference to last element in queue
     */
    T& back() {
        assert(len <= sz);
        return q[tail];
    }
    /**
     * remove front of queue and return it
     *
     * does not check that there's something in the queue
     * guard with `if (size()) ...` or `if (!empty()) ...`
     */
    T pop() override {
        assert(len != 0);
        auto ix = head;
        ++head;
        --len;
        // let queue always start at the beginning
        // if we ever get it empty
        if (len == 0) {
            head = {0};
            tail = {0};
        }
        return std::move(q[ix]);
    }
    /**
     * return number of elements in queue
     */
    size_t size() {
        return len;
    }
    /**
     * return true if queue is empty
     */
    bool empty() override {
        return len == 0;
    }
    /**
     * return true if queue is full
     */
    bool full() {
        return len == sz;
    }
};
