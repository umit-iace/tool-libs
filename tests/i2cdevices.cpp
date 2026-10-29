#include <doctest/doctest.h>
#include <vector>
#include <dev/HMC5883L.h>
#include <dev/QMC5883L.h>
#include <dev/BMI160.h>

struct Bus : Sink<I2C::Request> {
    std::vector<I2C::Request> got;
    bool isfull = false;
    bool full() override { return isfull; }
    void push(I2C::Request &&rq) override { got.push_back(std::move(rq)); }
};

TEST_CASE("tool-libs: HMC5883L") {
    Bus bus;
    HMC5883L h{bus};
    REQUIRE(bus.got.size() == 3);
    CHECK(bus.got[2].mem == 0x02); // mode register
    CHECK(bus.got[2].data[0] == HMC5883L::CONT);

    h.callback(I2C::Request{
        .dev = &h,
        .data = {0xff, 0xff, 0x00, 0x05, 0x00, 0x00}, // X, Z, Y
        .opts = {.read = true, .mem = true},
        .mem = 0x3,
    });
    CHECK(h.Axes.x == -1);
    CHECK(h.Axes.z == 5);
    CHECK(h.Axes.y == 0);
    CHECK(h.heading == doctest::Approx(M_PI + h.declinationAngle));
}

TEST_CASE("tool-libs: QMC5883L") {
    Bus bus;
    QMC5883L q{bus};
    q.callback(I2C::Request{
        .dev = &q,
        .data = {0xff, 0xff, 0, 0, 0, 0, 0, 0, 0}, // x = -1
        .opts = {.read = true, .mem = true},
        .mem = 0,
    });
    CHECK(q.getHeadingDegree() == doctest::Approx((M_PI + 0.0303) * 180 / M_PI));

    size_t before = bus.got.size();
    bus.isfull = true;
    q.measure();
    CHECK(bus.got.size() == before);
}

TEST_CASE("tool-libs: BMI160") {
    Bus bus;
    BMI160 b{bus};
    b.measure();
    auto &rq = bus.got.back();
    CHECK((bool)rq.opts.read);
    CHECK((bool)rq.opts.mem);
    CHECK(rq.mem == 0xc);
    CHECK(rq.data.size == 12);
}
