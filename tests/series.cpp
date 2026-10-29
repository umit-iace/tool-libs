#include <doctest/doctest.h>
#include <comm/series.h>

TEST_CASE("tool-libs: series: unpack") {
    SeriesUnpacker<double> s;
    Frame f{20};
    f.pack<uint32_t>(3);
    f.pack(1.).pack(2.).pack(3.);
    auto b = s.unpack(f);
    REQUIRE(b);
    REQUIRE(b->len == 3);
    CHECK(b->at(0) == 1.);
    CHECK(b->at(2) == 3.);
}

TEST_CASE("tool-libs: series: ignore absurd length") {
    SeriesUnpacker<double> s;
    Frame f{20};
    f.pack<uint32_t>(1000000000);
    f.pack(1.);
    CHECK(s.unpack(f) == nullptr);
    CHECK(s.buf.size == 0);

    // next series is received normally
    Frame g{20};
    g.pack<uint32_t>(1);
    g.pack(4.);
    auto b = s.unpack(g);
    REQUIRE(b);
    CHECK(b->at(0) == 4.);
}
