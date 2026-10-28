#include <doctest/doctest.h>
#include <core/timed.h>

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
