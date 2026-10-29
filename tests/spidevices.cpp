#include <doctest/doctest.h>
#include <vector>
#include <dev/AS5145.h>
#include <dev/MAX31865.h>

struct Bus : Sink<SPI::Request> {
    std::vector<SPI::Request> got;
    bool full() override { return false; }
    void push(SPI::Request &&rq) override { got.push_back(std::move(rq)); }
};

TEST_CASE("tool-libs: AS5145: full range position") {
    Bus bus;
    AS5145<1> a{bus, DIO{}};
    // dummy bit, 12 bit position = 4095, no errors
    a.callback(SPI::Request{.dev = &a, .data = {0x7f, 0xf8, 0x00}, .dir = SPI::Request::MISO});
    CHECK(a.getVal() == 4095);
}

TEST_CASE("tool-libs: MAX31865: high resistance") {
    Bus bus;
    MAX31865 m{bus, DIO{}}; // PT100, Rref = 430
    // 300 Ohm: 300/430 * 2^15 = 22862, shifted left by one (fault bit)
    uint16_t raw = 22862 << 1;
    m.callback(SPI::Request{
        .dev = &m,
        .data = {MAX31865::REG_RTD, (uint8_t)(raw >> 8), (uint8_t)raw},
        .dir = SPI::Request::BOTH,
    });
    CHECK(m.tempLin() == doctest::Approx((300 - 100) / (100 * 0.00385055)).epsilon(0.001));
    CHECK(m.temp() > 500);
}
