/** @file MovingAverage.h
 *
 * Copyright (c) 2019 IACE
 */
#pragma once
#include "Queue.h"

/**
 * Template class that implements a moving average filter as fifo.
 * @tparam T data type of the filter
 * @tparam N size of the filter
 */
template<typename T, int N>
class MovingAverage {
    Queue<T, N> samples{};
    T total{};
public:
    /**
     * Operator overload to return the average value.
     * @return the average value as double
     */
    double operator()() {
        return (double)total / samples.size();
    }

    /**
     * Operator overload to add a new value to the filter.
     * @param tSample new value
     */
    void operator()(T tSample) {
        total += tSample;
        if (samples.full()) {
            total -= samples.pop();
        }
        samples.push(tSample);
    }
};
