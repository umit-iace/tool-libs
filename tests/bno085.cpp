#include <doctest/doctest.h>
#include <string>
#include <vector>
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

struct Requests : Sink<I2C::Request> {
    std::vector<I2C::Request> got;
    bool full() override { return false; }
    void push(I2C::Request &&rq) override { got.push_back(std::move(rq)); }
};

static I2C::Request read(BNO085 &bno, std::initializer_list<uint8_t> data) {
    return I2C::Request{.dev = &bno, .data = data, .opts = {.read = true}};
}

TEST_CASE("tool-libs: BNO085: truncated reports aren't read beyond the data") {
    Bus bus;
    Collect c;
    BufferedLogger lg{c};
    BNO085 bno{bus};
    bno.lg = &lg;
    bno.acceleration = {1, 2, 3};
    // header says 8 bytes, accelerometer report needs 10
    bno.callback(read(bno, {8, 0, BNO085::C_NORM, 0, 0x01, 0, 0, 0}));
    CHECK(bno.acceleration.x == 1);
    // GIRV without the full payload
    bno.girv.pose.r = 7;
    bno.callback(read(bno, {6, 0, BNO085::C_GIRV, 0, 0x10, 0x20}));
    CHECK(bno.girv.pose.r == 7);
    lg.flush();
    CHECK(c.all.find("truncated Report") != std::string::npos);
}

TEST_CASE("tool-libs: BNO085: advertisement entry longer than the data") {
    Bus bus;
    Collect c;
    BufferedLogger lg{c};
    BNO085 bno{bus};
    bno.lg = &lg;
    // SHTP advertisement response: tag APP_NAME, claims 200 bytes, has 2
    bno.callback(read(bno, {8, 0, BNO085::C_SHTP, 0, BNO085::CMD_GET_ADV,
                            BNO085::APP_NAME, 200, 'h'}));
    lg.flush();
    CHECK(c.all.find("truncated advertisement") != std::string::npos);
}

TEST_CASE("tool-libs: BNO085: lengths from the device are limited") {
    Requests bus;
    BNO085 bno{bus};
    // 0x7fff bytes announced: read at most MAXREAD
    bno.callback(read(bno, {0xff, 0x7f, BNO085::C_NORM, 0}));
    REQUIRE(bus.got.size() == 1);
    CHECK(bus.got[0].data.size == BNO085::MAXREAD);
    // 0xffff is the error value
    bno.callback(read(bno, {0xff, 0xff, BNO085::C_NORM, 1}));
    CHECK(bus.got.size() == 1);
}

TEST_CASE("tool-libs: BNO085: stray FRS read response") {
    Bus bus;
    BNO085 bno{bus};
    REQUIRE(bno.qs.read.empty());
    // read response, status 3: record completed, nothing was requested
    bno.callback(read(bno, {20, 0, BNO085::C_CTRL, 0, 0xF3, 0x03,
                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
    CHECK(bno.qs.read.empty());
}

TEST_CASE("tool-libs: BNO085: sendMessage returns the sequence number it sent") {
    Requests bus;
    BNO085 bno{bus};
    bno.seq[BNO085::C_CTRL] = 7;
    uint8_t sq = bno.sendMessage({.chan = BNO085::C_CTRL, .cargo = {1, 2}});
    REQUIRE(bus.got.size() == 1);
    CHECK(sq == 7);
    CHECK(bus.got[0].data[3] == 7);
    CHECK(bno.seq[BNO085::C_CTRL] == 8);
}
