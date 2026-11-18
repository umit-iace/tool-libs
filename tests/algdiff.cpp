#include <doctest/doctest.h>
#include <utils/algdiff.h>

#include <cmath>
#include <numbers>

// first derivative, N = 0, alpha = beta = 2, T = 0.06 s, ts = 0.01 s, mid-point: delay 30 ms
static const std::array<double, 6> W1 = {
    9.9457504521, 14.6473779385, 6.3291139241, -6.3291139241, -14.6473779385, -9.9457504521
};

TEST_CASE("tool-libs: algdiff: a ramp is differentiated exactly") {
    AlgDiff<6> d{W1};
    double v = 0;
    for (int k = 0; k < 20; ++k) v = d(3 + 2 * 0.01 * k);
    CHECK(v == doctest::Approx(2));
}

TEST_CASE("tool-libs: algdiff: the first sample fills the history, also after reset") {
    AlgDiff<6> d{W1};
    CHECK(d(5) == doctest::Approx(0).epsilon(1e-9));
    for (int k = 0; k < 10; ++k) d(k);
    d.reset();
    CHECK(d(-7) == doctest::Approx(0).epsilon(1e-9));
}

TEST_CASE("tool-libs: algdiff: a sine is differentiated with the filter delay") {
    AlgDiff<6> d{W1};
    const double w = 2 * std::numbers::pi;  // 1 Hz, well below the 10 Hz cutoff
    double err = 0;
    for (int k = 0; k < 300; ++k) {
        double t = k * 0.01, v = d(std::sin(w * t));
        if (k >= 6) err = std::fmax(err, std::fabs(v - w * std::cos(w * (t - 0.03))));
    }
    CHECK(err < 0.05 * w);
}

struct V {
    double a, b;
    V operator+(const V &o) const { return {a + o.a, b + o.b}; }
    friend V operator*(double s, const V &v) { return {s * v.a, s * v.b}; }
};

TEST_CASE("tool-libs: algdiff: vector samples") {
    AlgDiff<6, V> d{W1};
    V v{};
    for (int k = 0; k < 10; ++k) v = d(V{0.01 * k, -0.03 * k});
    CHECK(v.a == doctest::Approx(1));
    CHECK(v.b == doctest::Approx(-3));
}
