/** @file RingBuffer.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

/**
 * Template class that implements a ring buffer.
 * @tparam T data type of the ring buffer
 * @tparam N size of the ring buffer
 */
template<typename T, int N>
class RingBuffer {
public:
    /**
     * Operator overload to return the value at the current set position.
     * @return the current value as double
     */
    double operator()() {
        return (double) this->tSamples[this->iIndex];
    }

    /**
     * Operator overload to return the value of last set position.
     * @return the current value as double
     */
    double getCurrent() {
        return (double) this->tSamples[this->iPrevIndex];
    }

    /**
     * Returns the current index of the ringbuffer.
     * @return index as int
     */
    uint16_t getCurrentIndex() {
        return iPrevIndex;
    }

    /**
     * Returns the value at the given index.
     * @param iIndex the index as int
     * @return the value at the index as double
     */
    double getAt(unsigned int iIndex) {
        if (iIndex <= 0) {
            return (double) this->tSamples[0];
        } else if (iIndex >= N) {
            return (double) this->tSamples[N];
        } else {
            return (double) this->tSamples[iIndex];
        }
    }

    /**
     * Operator overload to add a new value to the filter.
     * @param tSample new value
     */
    void operator()(T tSample) {
        this->tSamples[this->iIndex] = tSample;
        this->iPrevIndex = this->iIndex;
        this->iIndex = ++this->iIndex % N;
    }

    /**
     * Resets the ringbuffer to given value.
     */
    void reset(T tResetValue) {
        this->iIndex = 0;
        for (auto &tElem : this->tSamples) {
            tElem = tResetValue;
        }
    }

private:
    //\cond false
    T tSamples[N] = {};             ///< array to hold all needed samples
    uint16_t iIndex = 0;            ///< current index in array of all samples
    uint16_t iPrevIndex = 0;        ///< previous index in array of all samples
    //\endcond
};

#endif //RINGBUFFER_H
