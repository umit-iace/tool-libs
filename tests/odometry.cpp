#include <doctest/doctest.h>
#include <ctrl/odometry.h>

#include <initializer_list>
#include <numbers>

TEST_CASE("tool-libs: odometry: straight line") {
    Pose p{1, 2, std::numbers::pi / 2};
    for (int i = 0; i < 100; ++i) p = odometryStep(p, 0.5, 0, 0.01);
    CHECK(p.x == doctest::Approx(1));
    CHECK(p.y == doctest::Approx(2.5));
    CHECK(p.theta == doctest::Approx(std::numbers::pi / 2));
}

TEST_CASE("tool-libs: odometry: a full circle closes, also with coarse steps") {
    for (int steps : {4, 100, 1000}) {
        double T = 2 * std::numbers::pi;  // w = 1 rad/s
        Pose p{};
        for (int i = 0; i < steps; ++i) p = odometryStep(p, 0.3, 1, T / steps);
        CHECK(p.x == doctest::Approx(0).scale(1));
        CHECK(p.y == doctest::Approx(0).scale(1));
        CHECK(p.theta == doctest::Approx(T));
    }
}

TEST_CASE("tool-libs: odometry: small turn rates still turn") {
    Pose p{};
    for (int i = 0; i < 1000; ++i) p = odometryStep(p, 0.1, 1e-4, 0.01);
    CHECK(p.theta == doctest::Approx(1e-3));
    // radius 1000 m, arc of 1 m
    CHECK(p.y == doctest::Approx(1000 * (1 - std::cos(1e-3))));
}
