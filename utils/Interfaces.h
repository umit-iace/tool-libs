/** @file Interfaces.h
 *
 * Copyright (c) 2023 IACE
 * \todo should we provide empty or assert(false) implementations?
 */
#pragma once
#include <utility>

/// generic Interface for pushing objects around
/// can often nicely be implemented by stashing them in a queue
/// and handling them at a later time
template<typename T>
struct Push {
    /// copy semantics
    virtual void push(const T& t) {
        push(std::move(T{t}));
    }
    /// move semantics
    virtual void push(T&&)=0;
};

/// generic Interface for pulling objects around
template<typename T>
struct Pull {
    /// check if Pull interface is empty
    virtual bool empty()=0;
    /// pull object from interface
    [[nodiscard]] virtual T pop()=0;
};
