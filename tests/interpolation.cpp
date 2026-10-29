/** @file interpolation.cpp
 *
 * Copyright (c) 2020 IACE
 */

#include <doctest/doctest.h>
#include <ctrl/trajectory.h>

TEST_CASE("tool-libs: interpolation: linear") {
    double d[4] = {1,2, // x
                  2,4}; // y
    Buffer<double> dat{d, 4};
    LinearTrajectory li;
    SUBCASE("empty") {
        CHECK(li(0)[0] == 0);
        CHECK(li(1)[0] == 0);
    }

    li.setData(std::move(dat));

	SUBCASE("boundary") {
        CHECK(li(10)[0] == 4);
        CHECK(li(0)[0] == 2);
	}
	SUBCASE("interpolate") {
        CHECK(li(1.5)[0] == 3);
    	SUBCASE("update data") {
            double d2[4] = {2,3, // x
                          4,6}; // y
        	dat = Buffer{d2, 4};
        	li.setData(std::move(dat));
        	CHECK(li(2.5)[0] == 5);
    	}
    	SUBCASE("new data") {
            double d2[8] = {1,2,3,4, // x
                          2,4,2,4}; // y
        	dat = Buffer{d2, 8};
        	li.setData(std::move(dat));
            CHECK(li(0.5)[0] == 2);
            CHECK(li(1)[0] == 2);
            CHECK(li(1.5)[0] == 3);
            CHECK(li(2)[0] == 4);
            CHECK(li(2.5)[0] == 3);
            CHECK(li(3)[0] == 2);
            CHECK(li(3.5)[0] == 3);
            CHECK(li(4)[0] == 4);
            CHECK(li(4.5)[0] == 4);
    	}
        SUBCASE("multiple x of same y") {
            Buffer<double> data = {
                -1, -0.5, -0.1, 0.1, 0.5, 1,//x
                -1, -0.5,    0,   0, 0.5, 1 //y
            };
            li.setData(std::move(data));
            CHECK(li(-2)[0] == -1);
            CHECK(li(-1)[0] == -1);
            CHECK(li(-0.5)[0] == -0.5);
            INFO("y(-0.01)=",li(-0.01)[0]);
            INFO("y(-0)=",li(0)[0]);
            INFO("y(0.01)=",li(0.01)[0]);
            CHECK(li(0)[0] == 0);
            CHECK(li(0.5)[0] == 0.5);
            CHECK(li(1)[0] == 1);
            CHECK(li(2)[0] == 1);
        }
	}
}

TEST_CASE("tool-libs: interpolation: smooth") {
    // smootherstep: 35t^4 - 84t^5 + 70t^6 - 20t^7, value + 3 derivatives
    SmoothTrajectory st{{35, -84, 70, -20}, 7};
    SUBCASE("empty") {
        auto v = st(0.5);
        CHECK(v[0] == 0);
    }
    st.setData({0, 2, // x
                1, 3}); // y
    auto p = [](double t) { return ((( -20*t + 70)*t - 84)*t + 35)*t*t*t*t; };
    SUBCASE("boundary") {
        CHECK(st(-1)[0] == 1);
        CHECK(st(5)[0] == 3);
        CHECK(st(5)[1] == 0);
    }
    SUBCASE("value & derivatives") {
        double x = 0.7, h = 1e-3;
        auto y = [&](double x) { return 1 + 2 * p(x / 2); };
        auto v = st(x);
        CHECK(v[0] == doctest::Approx(y(x)));
        CHECK(v[1] == doctest::Approx((y(x+h) - y(x-h)) / (2*h)).epsilon(1e-4));
        double d2 = (y(x+h) - 2*y(x) + y(x-h)) / (h*h);
        CHECK(v[2] == doctest::Approx(d2).epsilon(1e-3));
        double d3 = (y(x+2*h) - 2*y(x+h) + 2*y(x-h) - y(x-2*h)) / (2*h*h*h);
        CHECK(v[3] == doctest::Approx(d3).epsilon(1e-2));
    }
}

TEST_CASE("tool-libs: interpolation: reference arithmetic") {
    Reference<2> a{1, 2}, b{10, 20};
    auto c = a + b;
    CHECK(c.y[0] == 11);
    CHECK(c.y[1] == 22);
    CHECK(a.y[0] == 1); // operands stay untouched
    auto d = b - a;
    CHECK(d.y[1] == 18);
    a += b;
    CHECK(a.y[0] == 11);
}
