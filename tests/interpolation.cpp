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
