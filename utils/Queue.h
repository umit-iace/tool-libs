#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#ifndef DEBUGLOG
#define DEBUGLOG(...)
#define UNDEF
#endif

/**
 * simple Array backed circle buffer queue to be used as FIFO
 */
template <typename T, int sz>
class Queue {
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
        size_t val;
        operator size_t() {
            return val;
        }
        size_t operator++(int) {
            size_t ret = val;
            val = (val + 1) % sz;
            return ret;
        }
    } head{}, tail{};
public:
    /**
     * copy element into queue
     */
    Queue& push(const T &val) {
        DEBUGLOG(stderr, "copying ..\n");
        return push(std::move(T{val}));
    }
    /**
     * move element into queue
     */
    Queue& push(T &&val) {
        DEBUGLOG(stderr, "moving ..\n");
        assert(len < sz);
        q[tail] = std::move(val);
        tail++;
        len++;
        return *this;
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
        assert(len != sz); // XXX: uhm, what? can this happen?
        return q[tail];
    }
    /**
     * remove front of queue and return it
     *
     * does not check that there's something in the queue
     */
    T pop() {
        assert(len != 0);
        auto ix = head;
        head++;
        len--;
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
    bool empty() {
        return len == 0;
    }
};
#ifdef UNDEF
#undef DEBUGLOG
#endif
