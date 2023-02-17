/** @file Interfaces.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <utility>

/// generic Interface for pushing objects around
/// can often nicely be implemented by stashing them in a queue
/// and handling them at a later time
template<typename T>
struct Push {
    //XXX: should we provide empty or assert(false) implementations?
    /// copy semantics
    virtual void push(const T& t) {
        push(std::move(T{t}));
    }
    virtual void push(T&&)=0;      //< move semantics
};

/// generic Interface for pulling objects around
template<typename T>
struct Pull {
    //XXX: should we provide empty or assert(false) implementations?
    virtual bool empty()=0;
    [[nodiscard]] virtual T pop()=0;
};
