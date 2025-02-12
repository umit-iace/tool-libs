/** @file streams.h
 *
 * Copyright (c) 2023 IACE
 * \todo should we provide empty or assert(false) implementations?
 */
#pragma once
#include <utility>

/** generic object sink, i.e. consumer of objects
 *
 * can often be implemented nicely by stashing them in a queue
 * and handling them at a later time
 */
template<typename T>
struct Sink {
    /// check if sink is full
    virtual bool full()=0;
    /// copy semantics
    /// does _not_ check for space. guard by using 'if (!full) { ... }'
    virtual void push(const T& t) {
        push(std::move(T{t}));
    }
    /// move semantics
    /// does _not_ check for space. guard by using 'if (!full) { ... }'
    virtual void push(T&&)=0;
    /// use this if you don't care if it's going to be successful.
    /// data gets discarded if sink is full.
    void trypush(T&& t) {
        if (full()) return;
        push(std::move(t));
    }
};

/** generic object source, i.e. generator of objects
 *
 * can often be implemented nicely by checking for / creating objects
 * into a queue on the `empty()` call and handing them out in the `pop()`
 */
template<typename T>
struct Source {
    /// check if source is empty
    virtual bool empty()=0;
    /// pull object from source
    /// does _not_ check for data. guard by using 'if (!empty) { ... }'
    virtual T pop()=0;
};
