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

TEST_CASE("tool-libs: series: values fill the frames, whatever their size") {
    SUBCASE("uint8_t") {
        SeriesUnpacker<uint8_t> s;
        Frame f{20};
        f.pack<uint32_t>(100);
        for (int i = 0; i < 76; ++i) f.pack<uint8_t>(i);
        CHECK(s.unpack(f) == nullptr);
        Frame g{20};
        for (int i = 76; i < 100; ++i) g.pack<uint8_t>(i);
        auto b = s.unpack(g);
        REQUIRE(b);
        REQUIRE(b->len == 100);
        bool same = true;
        for (int i = 0; i < 100; ++i) same &= b->at(i) == i;
        CHECK(same);
    }
    SUBCASE("uint16_t") {
        SeriesUnpacker<uint16_t> s;
        Frame f{20};
        f.pack<uint32_t>(40);
        for (int i = 0; i < 38; ++i) f.pack<uint16_t>(1000 + i);
        CHECK(s.unpack(f) == nullptr);
        Frame g{20};
        g.pack<uint16_t>(1038).pack<uint16_t>(1039);
        auto b = s.unpack(g);
        REQUIRE(b);
        CHECK(b->at(37) == 1037);
        CHECK(b->at(39) == 1039);
    }
    SUBCASE("short frame doesn't make up values") {
        SeriesUnpacker<double> s;
        Frame f{20};
        f.pack<uint32_t>(3);
        f.pack(1.);
        CHECK(s.unpack(f) == nullptr);
        CHECK(s.buf.len == 1);
    }
}
