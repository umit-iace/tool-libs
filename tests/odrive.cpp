#include <doctest/doctest.h>
#include <string>
#include <dev/ODrive.h>
void Kernel::idle() { }
Kernel k;

struct Out : Sink<Buffer<uint8_t>> {
    std::string last;
    int count = 0;
    bool isfull = false;
    bool full() override { return isfull; }
    void push(Buffer<uint8_t> &&b) override {
        last.assign((char*)b.buf, b.len);
        count++;
    }
};

/** response without '\0', followed by more digits in memory */
static Buffer<uint8_t> response(const char *s) {
    Buffer<uint8_t> b(128);
    memset(b.buf, '5', b.size);
    for (; *s; ++s) b.append(*s);
    return b;
}

TEST_CASE("tool-libs: ODrive") {
    Queue<Buffer<uint8_t>> in{5};
    Out out;
    ODrive od{.in = in, .out = out};
    ODrive::Motor m{.drive = &od, .side = 0};

    od.poll(); // k.time == 0 -> asks for voltage
    CHECK(out.last == "r vbus_voltage\n");

    in.push(response("24"));
    od.poll();
    REQUIRE(od.alive());

    SUBCASE("velocity command") {
        m.setSpeed(1.5);
        CHECK(out.last == "v 0 1.500 0\n");
    }
    SUBCASE("parse measurement") {
        m.measure();
        CHECK(out.last == "f 0\n");
        in.push(response("1.25 -2"));
        od.poll();
        CHECK(m.pos == 1.25);
        CHECK(m.vel == -2);
    }
    SUBCASE("output full") {
        out.isfull = true;
        int before = out.count;
        m.measure(); // must not hang
        CHECK(out.count == before);
        out.isfull = false;
        od.poll();
        CHECK(out.last == "f 0\n");
    }
    SUBCASE("unsolicited response") {
        in.push(response("1 2"));
        od.poll();
        CHECK(od.q.empty());
    }
}
