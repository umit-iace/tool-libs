#include <doctest/doctest.h>
#include <string>
#include <dev/BNO085.h>

struct Bus : Sink<I2C::Request> {
    bool full() override { return false; }
    void push(I2C::Request &&) override { }
};
struct Collect : Sink<Buffer<uint8_t>> {
    std::string all;
    bool full() override { return false; }
    void push(Buffer<uint8_t> &&b) override { all.append((char*)b.buf, b.len); }
};

TEST_CASE("tool-libs: BNO085: ignore unknown channel") {
    Bus bus;
    Collect c;
    BufferedLogger lg{c};
    BNO085 bno{bus};
    bno.lg = &lg;
    uint8_t seq[BNO085::C_LAST];
    memcpy(seq, bno.seq, sizeof seq);

    bno.callback(I2C::Request{
        .dev = &bno,
        .data = {4, 0, 0x42, 7}, // garbled header: channel 0x42
        .opts = {.read = true},
    });
    lg.flush();

    CHECK(memcmp(seq, bno.seq, sizeof seq) == 0);
    CHECK(c.all.find("unknown channel") != std::string::npos);
}

TEST_CASE("tool-libs: BNO085: acceleration report") {
    Bus bus;
    BNO085 bno{bus};
    // SHTP header (14 bytes, normal channel), then the accelerometer
    // report: id, seq, status, delay, x, y, z as Q8 m/s^2
    int16_t x = 256, y = -128, z = 9.81 * 256;
    bno.callback(I2C::Request{
        .dev = &bno,
        .data = {14, 0, BNO085::C_NORM, 0,
                 0x01, 0, 0, 0,
                 (uint8_t)x, (uint8_t)(x >> 8),
                 (uint8_t)y, (uint8_t)(y >> 8),
                 (uint8_t)z, (uint8_t)(z >> 8)},
        .opts = {.read = true},
    });
    CHECK(bno.acceleration.x == 1);
    CHECK(bno.acceleration.y == -0.5);
    CHECK(bno.acceleration.z == doctest::Approx(9.81).epsilon(0.005));
}
