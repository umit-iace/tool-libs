/** @file MovingAverageTest.cpp
 *
 * Copyright (c) 2019 IACE
 */

#define BOOST_TEST_MODULE MovingAverageTest

#include <cassert>
#include "MovingAverage.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( ConstValueTest ) {
    const int LEN = 5;
    MovingAverage<int, LEN> myMovingAverage;

    for (unsigned int i = 0; i < 2 * LEN; i++) {
        myMovingAverage(10);
        BOOST_CHECK_EQUAL(myMovingAverage(), 10);
    }

}

BOOST_AUTO_TEST_CASE( MovAvValueTest ) {
    const int LEN = 10;
    MovingAverage<int, LEN> myMovingAverage;

    for (unsigned int i = 0; i < LEN; i++) {
        myMovingAverage(i);
        BOOST_CHECK_EQUAL(myMovingAverage(), i/2.);
    }
}
