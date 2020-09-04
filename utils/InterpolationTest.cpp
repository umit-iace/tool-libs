/** @file InterpolationTest.cpp
 *
 * Copyright (c) 2020 IACE
 */

#define BOOST_TEST_MODULE InterpolationTest

#include "Interpolation.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( LinearInterpolatorBoundaryTest ) {
    const int LEN = 2;

    LinearInterpolator li({1, 2},{2, 4}, LEN);

    BOOST_CHECK_EQUAL(li(10), 4);
    BOOST_CHECK_EQUAL(li(0), 2);
}

BOOST_AUTO_TEST_CASE( LinearInterpolatorInterpolateTest ) {
    const int LEN = 2;

    LinearInterpolator li({1, 2},{2, 4}, LEN);

    BOOST_CHECK_EQUAL(li(1.5), 3);
}
