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

#include <dev/MAX31855.h>
TEST_CASE("tool-libs: MAX31855: negative temperatures") {
    Bus bus;
    MAX31855 m{bus, DIO{}};
    // -25°C thermocouple (14 bit, 0.25°C), 20°C internal (12 bit, 0.0625°C)
    uint32_t raw = (uint32_t)(-100 & 0x3fff) << 18 | (uint32_t)(320 & 0xfff) << 4;
    m.callback(SPI::Request{
        .dev = &m,
        .data = {(uint8_t)(raw >> 24), (uint8_t)(raw >> 16), (uint8_t)(raw >> 8), (uint8_t)raw},
        .dir = SPI::Request::MISO,
    });
    CHECK(m.temp() == -25);
    CHECK(m.internal() == 20);
}
