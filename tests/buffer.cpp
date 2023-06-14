#include <doctest/doctest.h>
#include <cstdio>
#include <utils/buffer.h>

/* void __assert_fail (const char *__assertion, const char *__file, */
/* 			   unsigned int __line, const char *__function){ */
/*     throw("oh no"); */
/*     while(1); */
/* } */

TEST_CASE("tool-libs: buffer") {
    Buffer<int> b = 256;
    for (int i = 0; i < 20; ++i) {
        b.append(i);
        CHECK(b[i] == i);
        CHECK(b.at(i) == i);
    }
}

TEST_CASE("tool-libs: buffer: out of range") {
    Buffer<int> b = 256;
    /* CHECK_THROWS(b[256]); */
    /* CHECK_THROWS(b.at(0)); */
    b.append(1);
    CHECK(b[0]);
    /* CHECK(b[1] == 0); /1* we don't initialize buffer memory anymore *1/ */
    CHECK(b.at(0));
    /* CHECK_THROWS(b.at(1)); */
}
