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
    SUBCASE("watchdog & recovery commands") {
        m.feed();
        CHECK(out.last == "u 0\n");
        od.clearErrors();
        CHECK(out.last == "sc\n");
        ODrive::Motor m1{.drive = &od, .side = 1};
        m1.request(ODrive::CLOSED_LOOP);
        CHECK(out.last == "w axis1.requested_state 8\n");
        CHECK(od.q.empty()); // none of them expects a response
    }
    SUBCASE("read axis state") {
        m.readState();
        CHECK(out.last == "r axis0.current_state\n");
        in.push(response("8"));
        od.poll();
        CHECK(m.state == 8);
        CHECK(od.q.empty());
        m.readState();
        in.push(response("invalid property")); // not an axis state
        od.poll();
        CHECK(m.state == 0);
    }
    SUBCASE("commands wait behind an unanswered request") {
        // one 100ms cycle of the HeliRack: measure, read state, feed, rearm
        ODrive::Motor m1{.drive = &od, .side = 1};
        m.measure(); m1.measure();
        m.readState(); m1.readState();
        m.feed(); m1.feed();
        od.clearErrors();
        m.setSpeed(0); m1.setSpeed(0);
        CHECK(od.alive());
        CHECK(out.last == "f 0\n"); // the rest waits for the answer
        in.push(response("1 2"));
        od.poll();
        CHECK(out.last == "f 1\n");
    }
    SUBCASE("unsolicited response") {
        in.push(response("1 2"));
        od.poll();
        CHECK(od.q.empty());
    }
}
