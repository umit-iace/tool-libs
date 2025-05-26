/** @file later.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <utility>

/** Defer Additions & Subtractions until access
 *
 * this is the poor man's closure for lambda functions
 *
 * e.g.
 * ```
 * int a{8}, b{3}, c{4};
 * auto op = (Later<int>) a + b - c;
 * op.get() // -> 7
 * a = 4;
 * op.get() // -> 3
 * ```
 */
template<typename T>
struct Later {
    T& where{};
    Later *nested{};
    enum Op {NOP, ADD, SUB} op{};
    ~Later() { delete nested; nested = nullptr; op = NOP; }
    Later(T& val): where(val) {} // unable to store temporaries. cast the first operand to this type first
    Later(T& v, Later *n, Op op): where(v), nested(n), op(op) {}
    // copying impossible. who'd own nested Laters?
    Later(const Later &)=delete;
    Later& operator=(const Later &)=delete;
    // moving is ok
    Later(Later &&l) noexcept : where(l.where), nested(l.nested), op(l.op) {
        l.nested = nullptr;
        l.op = NOP;
    }
    Later& operator=(Later &&l) noexcept {
        if (this == &l) return *this;
        new (this) Later(std::move(l));
        return *this;
    }
    Later operator+(T &val) {
        return {
            val,
            ::new Later{std::move(*this)},
            ADD,
        };
    }
    Later operator-(T &val) {
        return {
            val,
            ::new Later{std::move(*this)},
            SUB,
        };
    }
    /** get value */
    T get() const {
        switch (op) {
        case ADD: return nested->get() + where;
        case SUB: return nested->get() - where;
        default: return where;
        }
    }
    // placement new for copying into place
    void *operator new(size_t sz, Later *where) {
        where->~Later();
        return where;
    }
};
