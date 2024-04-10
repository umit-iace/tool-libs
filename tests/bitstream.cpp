#include <doctest/doctest.h>
#include <iostream>

#include <utils/bitstream.h>
struct Data {
    uint8_t cnf:6;
    int16_t pos:12;
    uint8_t ign:1;
    bool operator ==(Data &o) {
        return cnf == o.cnf && pos == o.pos && ign == o.ign;
    }
};

TEST_CASE("tool-libs: bitstream") {
    int const cases = 1;
    uint8_t indata[cases][3] = {
        {0xbf, 0xf8, 0xe0}
    };
    Data out[cases] = {
        {.cnf = 7, .pos = 2047, .ign = 1}
    };
    for (int c = 0; c < cases; ++c) {
        BitStream bs {indata[c]};
        Data data;
        data.ign = bs.range<decltype(data.ign)>(0, 1);
        data.pos = bs.range<decltype(data.pos)>(1, 13);
        data.cnf = bs.range<decltype(data.cnf)>(13, 19);
        CHECK(data == out[c]);
    }

}

TEST_CASE("tool-libs: bistream: long") {
    int const cases = 1;
    uint8_t indata[1][10] = {
        {0x3f, 0xf0, 0xe0, 0xff, 0xf0, 0xff, 0x0f,0x0f, 0xf0, 0xf0},
    };
    Data out[cases][4] = {
        {
            {.cnf = 7, .pos = 2046, .ign = 0},
            {.cnf = 0xf<<2, .pos = 0xff, .ign = 0},
            {.cnf = 0xf<<1, .pos = 0xff<<3, .ign = 0},
            {.cnf = 0xf, .pos = 0xff<<2, .ign = 0},
        }
    };
    for (int c = 0; c < cases; ++c) {
        BitStream bs{indata[c]};
        for (int i = 0; i < 3; ++i) {
            Data data;
            data.ign = bs.range<decltype(data.ign)>(i*19 + 0, i*19 + 1);
            data.pos = bs.range<decltype(data.pos)>(i*19 + 1, i*19 + 13);
            data.cnf = bs.range<decltype(data.cnf)>(i*19 + 13, i*19 + 19);
            CHECK(data == out[c][i]);
        }
    }
}
