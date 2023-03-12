#define assert(b) do{if (!(b)) throw("ohno");} while(0)
#include <doctest/doctest.h>
#include <cstdio>
#include <utils/Buffer.h>

TEST_CASE("tool-libs: buffer") {
    Buffer<int> b {256};
    for (int i = 0; i < 20; ++i) {
        b.append(i);
        CHECK(b[i] == i);
        CHECK(b.at(i) == i);
    }
}

TEST_CASE("tool-libs: buffer: out of range") {
    Buffer<int> b{256};
    CHECK_THROWS(b[256]);
    CHECK_THROWS(b.at(0));
    b.append(1);
    CHECK(b[0]);
    CHECK(b[1] == 0);
    CHECK(b.at(0));
    CHECK_THROWS(b.at(1));
}
