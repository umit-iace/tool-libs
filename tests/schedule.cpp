#include <doctest/doctest.h>
#include <core/timed.h>
#include <utils/deadline.h>
#include <type_traits>

static int calls;
static void count(uint32_t, uint32_t) { calls++; }

TEST_CASE("tool-libs: schedule: registry grows beyond initial size") {
    calls = 0;
    Schedule::Recurring::Registry r;
    for (int i = 0; i < 50; ++i) r.every(10, count);
    CHECK(r.list.len == 50);
    Scheduler s;
    s.q = Queue<Schedule::Schedulable*>{100};
    CHECK(s.schedule(0, r));
    s.run();
    CHECK(calls == 50);
}

TEST_CASE("tool-libs: schedule: recurring keeps its period") {
    calls = 0;
    Schedule::Recurring::Registry r;
    r.every(100, count);
    Scheduler s;
    for (uint32_t t = 0; t < 1000; ++t) {
        s.schedule(t, r);
        s.run();
    }
    CHECK(calls == 10);
}

TEST_CASE("tool-libs: schedule: recurring doesn't catch up on missed calls") {
    calls = 0;
    Schedule::Recurring::Registry r;
    r.every(100, count);
    Scheduler s;
    // registered late
    for (uint32_t t = 60000; t < 60100; ++t) {
        s.schedule(t, r);
        s.run();
    }
    CHECK(calls == 1);
}

TEST_CASE("tool-libs: schedule: recurring across the uint32_t wrap-around") {
    calls = 0;
    Schedule::Recurring::Registry r;
    r.every(100, count);
    Scheduler s;
    uint32_t t = 0xffffffffu - 450;
    for (int i = 0; i < 1000; ++i, ++t) {
        s.schedule(t, r);
        s.run();
    }
    CHECK(calls == 10);
}

TEST_CASE("tool-libs: schedule: deadline across the uint32_t wrap-around") {
    uint32_t now = 0xffffffffu - 5;
    auto d = Deadline::after(now, 10);
    CHECK(d.when == 4);
    CHECK_FALSE(d(now));
    CHECK_FALSE(d(now + 9));
    CHECK(d(now + 10));
    CHECK_FALSE(d(now + 11)); // fires once

    // landing exactly on 0 must not disable it
    auto z = Deadline::after(0xffffffffu, 1);
    CHECK(z.when != 0);
    CHECK(z(1));
}

TEST_CASE("tool-libs: schedule: remove drops pending calls") {
    calls = 0;
    Schedule::Recurring::Registry r;
    auto a = r.every(10, count);
    auto b = r.every(10, count);
    REQUIRE(a);
    CHECK(r.every(0, count) == nullptr);
    Scheduler s;
    s.schedule(0, r); // both queued
    CHECK(r.remove(a, s));
    CHECK_FALSE(r.remove(a, s)); // gone already
    s.run();
    CHECK(calls == 1);
    CHECK(r.list.len == 1);
    CHECK(r.list.buf[0] == b);
}

namespace {
Schedule::Recurring::Registry selfreg;
Scheduler selfsched;
struct OneShot {
    Schedule::Schedulable *handle{};
    int n{};
    void call(uint32_t, uint32_t) {
        ++n;
        selfreg.remove(handle, selfsched); // destroys the running Schedulable
    }
};
}
TEST_CASE("tool-libs: schedule: a recurring call can remove itself") {
    OneShot o;
    o.handle = selfreg.every(1, o, &OneShot::call);
    for (uint32_t t = 0; t < 5; ++t) {
        selfsched.schedule(t, selfreg);
        selfsched.run();
    }
    CHECK(o.n == 1);
    CHECK(selfreg.list.len == 0);
}

TEST_CASE("tool-libs: schedule: registries own their entries, no copies") {
    static_assert(!std::is_copy_constructible_v<Schedule::Registry>);
    static_assert(!std::is_copy_assignable_v<Schedule::Recurring::Registry>);
}
