/** @file InterpolationTest.cpp
 *
 * Copyright (c) 2020 IACE
 */

#define BOOST_TEST_MODULE DynamicArrayTest

#include "DynamicArray.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( DynamicArrayDoubleTest ) {
    DynamicArray<double> DA;
    double testDouble = 5.3;
    DA.push_back(testDouble);
    DA.push_back(testDouble);
    double dx[2] = {1, 2};
    DA.push_back(dx[0]);
    DA.push_back(dx[1]);
    double dy[2] = {2, 4};
    DA.push_back(dy[0]);
    DA.push_back(dy[1]);

    BOOST_CHECK_EQUAL(DA[0], testDouble);
    BOOST_CHECK_EQUAL(DA[1], testDouble);
    BOOST_CHECK_EQUAL(DA[2], dx[0]);
    BOOST_CHECK_EQUAL(DA[3], dx[1]);
    BOOST_CHECK_EQUAL(DA[4], dy[0]);
    BOOST_CHECK_EQUAL(DA[5], dy[1]);
    DA.pop_back();
    DA.pop_back();
    DA.pop_back();
    DA.pop_back();
    BOOST_CHECK_EQUAL(DA[1], testDouble);
}
