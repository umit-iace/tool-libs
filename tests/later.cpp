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
