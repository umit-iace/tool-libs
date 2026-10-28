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
