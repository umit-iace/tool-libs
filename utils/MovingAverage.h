/** @file MovingAverage.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H
/**
 * Template class that implements a moving average filter as fifo.
 * @tparam T data type of the filter
 * @tparam N size of the filter
 */
template<typename T, int N>
class MovingAverage {
public:
    /**
     * Operator overload to return the average value.
     * @return the average value as double
     */
    double operator()() {
        if (!bInit)
            return (double) tTotal / N;
        if (!iIndex)
            return 0;
        return (double) tTotal / iIndex;
    }

    /**
     * Operator overload to add a new value to the filter.
     * @param tSample new value
     */
    void operator()(T tSample) {
        tTotal += tSample - tSamples[iIndex];
        tSamples[iIndex] = tSample;
        iIndex = ++iIndex % N;
        if (!iIndex)
            bInit = false;
    }

private:
    //\cond false
    T tSamples[N] = {};             ///< array to hold all needed samples
    unsigned int iIndex = 0;        ///< current index in array of all samples
    T tTotal = 0;                   ///< summed total value
    bool bInit = true;              ///< flag to recognize initialization
    //\endcond
};

#endif //MOVINGAVERAGE_H
