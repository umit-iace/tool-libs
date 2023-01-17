#pragma once

/**
 * simple Array backed circle buffer queue to be used as FIFO
 *
 * XXX: currently only works as used in Min.h ===> only actually marks
 * the last element as done and moves on
 */
template <typename T, int sz>
struct Queue {
    T q[sz];
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

    bool empty() {
        return len == 0;
    }
    void push(const T &val) {
        assert(len < sz);
        /* q[tail++] = T{val}; */
        tail++;
        len++;
    }
    T front() {
        assert(len != 0);
        return std::move(q[head]);
    }
    T& back() {
        assert(len != sz);
        return q[tail];
    }
    void pop() {
        assert(len != 0);
        /* if (len != 0) { */
            head++;
            len--;
        /* } */
    }
};
