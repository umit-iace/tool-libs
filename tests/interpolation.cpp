/** @file interpolation.cpp
 *
 * Copyright (c) 2020 IACE
 */

#include <doctest/doctest.h>
#include <ctrl/trajectory.h>


TEST_CASE("tool-libs: interpolation: step") {
    double d[4] = {1,2, // x
                  2,4}; // y
    Buffer<double> dat{d, 4};
    StepTrajectory stp;
    SUBCASE("empty") {
        CHECK(stp(0) == 0);
        CHECK(stp(1) == 0);
    }

    stp.setData(std::move(dat));

	SUBCASE("boundary") {
        CHECK(stp(10) == 4);
        CHECK(stp(0) == 2);
	}
	SUBCASE("interpolate") {
        CHECK(stp(1.5) == 2);
    	SUBCASE("update data") {
            double d2[4] = {2,3, // x
                          4,6}; // y
        	dat = Buffer{d2, 4};
        	stp.setData(std::move(dat));
        	CHECK(stp(2.5) == 4);
    	}
    	SUBCASE("new data") {
            double d2[8] = {1,2,3,4, // x
                          2,4,2,4}; // y
        	dat = Buffer{d2, 8};
        	stp.setData(std::move(dat));
            CHECK(stp(0.5) == 2);
            CHECK(stp(1) == 2);
            CHECK(stp(1.5) == 2);
            CHECK(stp(2) == 4);
            CHECK(stp(2.5) == 4);
            CHECK(stp(3) == 2);
            CHECK(stp(3.5) == 2);
            CHECK(stp(4) == 4);
            CHECK(stp(4.5) == 4);
    	}
	}
}

TEST_CASE("tool-libs: interpolation: linear") {
    double d[4] = {1,2, // x
                  2,4}; // y
    Buffer<double> dat{d, 4};
    LinearTrajectory li;
    SUBCASE("empty") {
        CHECK(li(0) == 0);
        CHECK(li(1) == 0);
    }

    li.setData(std::move(dat));

	SUBCASE("boundary") {
        CHECK(li(10) == 4);
        CHECK(li(0) == 2);
	}
	SUBCASE("interpolate") {
        CHECK(li(1.5) == 3);
    	SUBCASE("update data") {
            double d2[4] = {2,3, // x
                          4,6}; // y
        	dat = Buffer{d2, 4};
        	li.setData(std::move(dat));
        	CHECK(li(2.5) == 5);
    	}
    	SUBCASE("new data") {
            double d2[8] = {1,2,3,4, // x
                          2,4,2,4}; // y
        	dat = Buffer{d2, 8};
        	li.setData(std::move(dat));
            CHECK(li(0.5) == 2);
            CHECK(li(1) == 2);
            CHECK(li(1.5) == 3);
            CHECK(li(2) == 4);
            CHECK(li(2.5) == 3);
            CHECK(li(3) == 2);
            CHECK(li(3.5) == 3);
            CHECK(li(4) == 4);
            CHECK(li(4.5) == 4);
    	}
	}
}
