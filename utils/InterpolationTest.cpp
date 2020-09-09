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

BOOST_AUTO_TEST_CASE( LinearInterpolatorUpdateDataTest ) {

    double dx[2] = {1, 2};
    double dy[2] = {2, 4};
    LinearInterpolator li(dx, dy, 2);

    BOOST_CHECK_EQUAL(li(1.5), 3);

    double dxN[2] = {2, 3};
    double dyN[2] = {4, 6};

    li.updateData(dxN, dyN);
    BOOST_CHECK_EQUAL(li(2.5), 5);
}

BOOST_AUTO_TEST_CASE( LinearInterpolatorInitUpdateDataTest ) {

    double dx[0] = {};
    double dy[0] = {};
    LinearInterpolator li(dx, dy, 2);

    double dxN[2] = {2, 3};
    double dyN[2] = {4, 6};

    li.updateData(dxN, dyN);
    BOOST_CHECK_EQUAL(li(2.5), 5);
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
    BOOST_CHECK_EQUAL(li(1), 0);

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
