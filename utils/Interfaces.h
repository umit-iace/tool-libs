/** @file Interfaces.h
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
    /// copy semantics
    virtual void push(const T& t) {
        push(std::move(T{t}));
    }
    /// move semantics
    virtual void push(T&&)=0;
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
    [[nodiscard]] virtual T pop()=0;
};
