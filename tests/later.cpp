#include <cstdlib>
#include <cstdint>
#include <utils/later.h>
#include <doctest/doctest.h>

struct C {
    double d{5.4};
    operator double&() {
        return d;
    }
};

using doctest::Approx;
TEST_CASE("tool-libs: later: ") {
    C c{3}, b{2};
    double a{3.4};
    Later<double> d = (Later<double>)
        a + c - b;
    CHECK(d.get() == Approx(a+c-b));
    c.d = 4;
    b.d = 0.1;
    CHECK(d.get() == Approx(a+c-b));

    SUBCASE("simple") {
        d = Later<double>(a);
        CHECK(d.get() == Approx(a));
        a = 5.2;
        CHECK(d.get() == Approx(a));
    }
    SUBCASE("reassigning") {
        d = (Later<double>)c+b;
        CHECK(d.get() == Approx(c+b));
        c.d = 3.33;
        CHECK(d.get() == Approx(c+b));
        double ha = 1;
        d = d - ha;
        CHECK(d.get() == Approx(c+b-ha));
    }
    SUBCASE("moving") {
        double cnst = 4.5;
        Later<double> e {cnst};
        CHECK(e.get() == Approx(4.5));
        e = std::move(d);
        CHECK(d.get() == Approx(0.1));
    }
}
TEST_CASE("tool-libs: later: in container") {
    double a{3.4}, b{5.4};
    struct Container {
        Later<double> d;
    } c {.d = (Later<double>)a};
    CHECK(c.d.get() == Approx(a));
    c.d = (Later<double>)b - a;
    CHECK(c.d.get() == Approx(b-a));
}

struct Model {
    double a{3.3}, b{4.5};
    Later<double> l = (Later<double>)a - b;
    void reset() {
        new(this) Model{};
    }
    void *operator new(size_t sz, Model *where) {
        where->~Model();
        return where;
    }
};
TEST_CASE("tool-libs: later: reset") {
    Model m;
    CHECK(m.l.get() == Approx(m.a - m.b));
    m.l = (Later<double>)m.a + m.b;
    CHECK(m.l.get() == Approx(m.a + m.b));
    m.reset();
    CHECK(m.l.get() == Approx(m.a - m.b));
}

