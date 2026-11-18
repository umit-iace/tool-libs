#include <doctest/doctest.h>
#include <ctrl/bspline.h>

static Buffer<double> series(std::initializer_list<double> l) {
    Buffer<double> b(l.size());
    for (double v : l) b.append(v);
    return b;
}

TEST_CASE("tool-libs: bspline: single cubic segment is x^3") {
    BSplineTrajectory s(5);
    // Bernstein coefficients of x^3 on [0, 1]
    s.setData(series({3, 8, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1}));
    for (double x : {0.0, 0.25, 0.5, 1.0}) {
        auto &d = s(x);
        CHECK(d[0] == doctest::Approx(x * x * x));
        CHECK(d[1] == doctest::Approx(3 * x * x));
        CHECK(d[2] == doctest::Approx(6 * x));
        CHECK(d[3] == doctest::Approx(6));
        CHECK(d[4] == 0);
    }
}

TEST_CASE("tool-libs: bspline: matches scipy make_interp_spline, k=5, rest at both ends") {
    BSplineTrajectory s(4);
    s.setData(series({5, 14,
                      0, 0, 0, 0, 0, 0, 1, 2.5, 4, 4, 4, 4, 4, 4,
                      0, 0, 0, 4.691325866110965, -5.54762524963098, 2.0000000000000004, 2, 2}));
    struct { double x, d[4]; } ref[] = {
        {0, {0, 0, 0, 28.1479551966658}},
        {0.3, {0.0884941703568637, 0.769519927064339, 3.6699557176348, -1.05558739255015}},
        {1, {1.0, 0.767560996787357, -4.4768602934792, -7.92393852565772}},
        {1.7, {0.413490820504739, -2.00011667240161, -1.04063742594147, 12.5630851655684}},
        {3.2, {0.992761127565585, 2.63389441480495, -1.57172960636826, -12.4387973977277}},
        {4, {2.0, 0, 0, 25.1587508321033}},
    };
    CHECK(s.begin() == 0);
    CHECK(s.end() == 4);
    for (auto &r : ref) {
        auto &d = s(r.x);
        for (int j = 0; j < 4; ++j) CHECK(d[j] == doctest::Approx(r.d[j]).epsilon(1e-9));
    }
    SUBCASE("holds the ends at rest") {
        auto &a = s(-1);
        CHECK(a[0] == 0);
        CHECK(a[3] == 0);
        auto &b = s(10);
        CHECK(b[0] == doctest::Approx(2));
        CHECK(b[1] == 0);
        CHECK(b[3] == 0);
    }
}

TEST_CASE("tool-libs: bspline: invalid data clears the spline") {
    BSplineTrajectory s(2);
    s.setData(series({3, 8, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1}));
    CHECK(s(1)[0] == doctest::Approx(1));
    SUBCASE("wrong length") { s.setData(series({3, 8, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0})); }
    SUBCASE("decreasing knots") { s.setData(series({3, 8, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1})); }
    SUBCASE("degree 0") { s.setData(series({0, 2, 0, 1, 5})); }
    SUBCASE("NaN knot") { s.setData(series({3, 8, 0, 0, 0, 0, NAN, 1, 1, 1, 0, 0, 0, 1})); }
    CHECK(s(1)[0] == 0);
    CHECK(s.end() == 0);
}
