#pragma once

/** Deferred Additions & Subtractions until access
 *
 * this is the poor man's closure for lambda functions
 *
 * e.g.
 * ```
 * int a{8}, b{3}, c{4};
 * auto op = (Later<int>) a + b - c;
 * op.get() /// -> 7
 * a = 4;
 * op.get() /// -> 3
 * ```
 */
template<typename T>
struct Later {
    T& where{};
    Later *nested{};
    enum Op {NOP, ADD, SUB} op{};
    ~Later() { delete nested; nested = nullptr;}
    Later(T& val): where(val) {} // unable to store temporaries. cast the first operand to this type first
    Later(T& v, Later *n, Op op): where(v), nested(n), op(op) {}
    // copying impossible. who'd own nested Laters?
    Later(const Later &)=delete;
    Later& operator=(const Later &)=delete;
    // moving is ok
    Later(Later &&l) noexcept : where(l.where), nested(l.nested), op(l.op) {
        l.nested = nullptr;
    }
    Later& operator=(Later &&l) noexcept {
        this->~Later();
        where = l.where;
        nested = l.nested;
        op = l.op;
        l.nested = nullptr;
        return *this;
    }
    Later operator+(T &val) {
        return {
            val,
            new Later{std::move(*this)},
            ADD,
        };
    }
    Later operator-(T &val) {
        return {
            val,
            new Later{std::move(*this)},
            SUB,
        };
    }
    /** implicit type conversion access */
    constexpr operator T() const {
        switch (op) {
        case NOP: return where;
        case ADD: return (T)*nested + where;
        case SUB: return (T)*nested - where;
        }
    }
    /** explicit accessor */
    T get() const {
        return (T)*this;
    }
};
