#pragma once

/** Deferred Additions & Subtractions until access
 *
 * this is the poor man's closure for lambda functions
 *
 * e.g.
 * ```
 * int a{8}, b{3}, c{4};
 * auto op = Later(a).plus(b).minus(c);
 * op.get() /// -> 7
 * a = 4;
 * op.get() /// -> 3
 * ```
 */
template<typename T>
struct Later {
    const T& where{};
    const Later *nested{};
    enum Op {NOP, ADD, SUB} op{};
    ~Later() { delete nested; nested = nullptr;}
    Later(T& val): where(val) {}
    Later(T& v, Later *n, Op op): where(v), nested(n), op(op) {}
    Later(Later &l) : where(l.where), nested(l.nested), op(l.op) {
        l.nested = nullptr;
    }
    Later(Later &&l)=delete;
    Later& operator=(Later l)=delete;
    Later& operator=(Later &&l)=delete;
    Later plus(T &val) {
        return {
            val,
            new Later{*this},
            ADD,
        };
    }
    Later minus(T &val) {
        return {
            val,
            new Later{*this},
            SUB,
        };
    }
    constexpr operator T() const {
        switch (op) {
        case NOP: return where;
        case ADD: return *nested + where;
        case SUB: return *nested - where;
        }
    }
    T get() const {
        return (T)*this;
    }
};
