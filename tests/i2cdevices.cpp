#include <doctest/doctest.h>
#include <vector>
#include <dev/HMC5883L.h>
#include <dev/QMC5883L.h>
#include <dev/BMI160.h>

void Kernel::idle() { }
Kernel k;
static void tickrun(int ms) {
    for (int i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

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

TEST_CASE("tool-libs: BMI160: power up commands are spaced out") {
    Bus bus;
    BMI160 b{bus};
    auto cmds = [&]{
        int n = 0;
        for (auto &rq : bus.got) n += rq.mem == 0x7e;
        return n;
    };
    CHECK(cmds() == 1); // accelerometer only
    tickrun(4);
    CHECK(cmds() == 1); // accelerometer needs 3.8ms
    tickrun(4);
    CHECK(cmds() == 2);
    CHECK(bus.got.back().data[0] == 0x15); // gyroscope
    CHECK_FALSE(b.ready());
    tickrun(90);
    CHECK(b.ready());
    size_t registered = k.list.len;
    tickrun(10);
    CHECK(k.list.len == registered); // startup helper is gone
}

TEST_CASE("tool-libs: BMI160: unregisters when destroyed early") {
    Bus bus;
    size_t before = k.list.len;
    {
        BMI160 b{bus};
        CHECK(k.list.len == before + 1);
    }
    CHECK(k.list.len == before);
    tickrun(10);
}

TEST_CASE("tool-libs: I2C request kinds from the option bits") {
    I2C::Request::Type t{.read = 1, .slave = 0, .mem = 1};
    CHECK(t.type() == I2C::Request::Type::MEM_READ);
    for (auto k : {I2C::Request::Type::MASTER_WRITE, I2C::Request::Type::MASTER_READ,
                   I2C::Request::Type::SLAVE_WRITE, I2C::Request::Type::SLAVE_READ,
                   I2C::Request::Type::MEM_WRITE, I2C::Request::Type::MEM_READ}) {
        t.set(k);
        CHECK(t.type() == k);
    }
}
