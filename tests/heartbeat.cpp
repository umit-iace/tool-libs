#include <doctest/doctest.h>
#include <core/experiment.h>
// time is advanced manually in this test
void Kernel::idle() { }
Kernel k;

static uint32_t timedOutAt;

static void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

TEST_CASE("tool-libs: experiment: heartbeat timeout relative to start") {
    FrameRegistry reg;
    Experiment e{&reg};
    Queue<Frame> notify{5};
    e.onEvent(e.TIMEOUT).call([](uint32_t t) { timedOutAt = t; });
    e.setHeartbeatTimeout(100, notify);

    step(1000);
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b10))); // heartbeat while IDLE
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b01))); // start experiment
    step(2000);

    CHECK(e.state == e.IDLE); // timed out
    CHECK(timedOutAt > 0);
    CHECK(timedOutAt <= 110);
}
