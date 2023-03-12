#include <doctest/doctest.h>
#include <cassert>
#include <utils/MovingAverage.h>


TEST_CASE("tool-libs: movingaverage: ConstValueTest") {
    const int LEN = 5;
    MovingAverage<int, LEN> myMovingAverage;

    for (unsigned int i = 0; i < 2 * LEN; i++) {
        myMovingAverage(10);
        CHECK(myMovingAverage() == 10);
    }

}

TEST_CASE("tool-libs: movingaverage: MovAvValueTest") {
    const int LEN = 10;
    MovingAverage<int, LEN> myMovingAverage;

    for (unsigned int i = 0; i < LEN; i++) {
        myMovingAverage(i);
        CHECK(myMovingAverage() == i/2.);
    }
}
