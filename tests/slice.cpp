#include <doctest/doctest.h>
#include <utils/slice.h>

TEST_CASE("tool-libs: slice: slice of slice") {
    Buffer<int> b{1,2,3,4,5,6,7,8,9,10};
    Slice<int> s{b, 2, 5};
    REQUIRE(s.len == 3);
    CHECK(s[0] == 3);

    Slice<int> rest{s, 1};
    REQUIRE(rest.len == 2);
    CHECK(rest[0] == 4);
    CHECK(rest[1] == 5);

    Slice<int> mid{s, 1, 2};
    REQUIRE(mid.len == 1);
    CHECK(mid[0] == 4);
}
