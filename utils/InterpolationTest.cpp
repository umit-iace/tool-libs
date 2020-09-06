/** @file InterpolationTest.cpp
 *
 * Copyright (c) 2020 IACE
 */

#define BOOST_TEST_MODULE InterpolationTest

#include "Interpolation.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( LinearInterpolatorBoundaryTest ) {

    double dx[2] = {1, 2};
    double dy[2] = {2, 4};
    LinearInterpolator li(dx, dy, 2);

    BOOST_CHECK_EQUAL(li(10), 4);
    BOOST_CHECK_EQUAL(li(0), 2);
}

BOOST_AUTO_TEST_CASE( LinearInterpolatorInterpolateTest ) {

    double dx[2] = {1, 2};
    double dy[2] = {2, 4};
    LinearInterpolator li(dx, dy, 2);

    BOOST_CHECK_EQUAL(li(1.5), 3);
}

BOOST_AUTO_TEST_CASE( LinearInterpolatorSetDataTest ) {

    double dx[2] = {1, 2};
    double dy[2] = {2, 4};
    LinearInterpolator li(dx, dy, 2);

    BOOST_CHECK_EQUAL(li(1.5), 3);

    double dxN[4] = {1, 2, 3, 4};
    double dyN[4] = {2, 4, 2, 4};
    li.setData(dxN, dyN, 4);
    BOOST_CHECK_EQUAL(li(0.5), 2);
}


BOOST_AUTO_TEST_CASE( LinearInterpolatorChangeDataTest ) {

    LinearInterpolator li;

    double dxN[4] = {1, 2, 3, 4};
    double dyN[4] = {2, 4, 2, 4};
    li.setData(dxN, dyN, 4);
    BOOST_CHECK_EQUAL(li(0.5), 2);
}

BOOST_AUTO_TEST_CASE( LinearInterpolatorInterpolateMultipleTest ) {

    double dx[4] = {1, 2, 3, 4};
    double dy[4] = {2, 4, 2, 4};
    LinearInterpolator li(dx, dy, 4);

    BOOST_CHECK_EQUAL(li(0.5), 2);
    BOOST_CHECK_EQUAL(li(1),2);
    BOOST_CHECK_EQUAL(li(1.5), 3);
    BOOST_CHECK_EQUAL(li(2),4);
    BOOST_CHECK_EQUAL(li(2.5), 3);
    BOOST_CHECK_EQUAL(li(3),2);
    BOOST_CHECK_EQUAL(li(3.5), 3);
    BOOST_CHECK_EQUAL(li(4),4);
    BOOST_CHECK_EQUAL(li(4.5), 4);
}
