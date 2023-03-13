#include <doctest/doctest.h>
#include <core/experiment.h>
// minimal Kernel idle implementation
void Kernel::idle() {
    tick(1);
};
Kernel k;

void shout(uint32_t time, uint32_t dt) {
    static int count = 0;
    CHECK(dt == 7);
    CHECK(time % dt == 0);
    CHECK(time / dt == count++);
}
struct Shouter {
    int count = 0;
    void shout(uint32_t time, uint32_t dt) {
        CHECK(dt == 8);
        CHECK(time % dt == 0);
        CHECK(time / dt == count++);
    }
};

TEST_CASE("shouting") {
    Experiment e{};
    Shouter s{};
    e.during(e.IDLE).every(7, shout);
    e.during(e.IDLE).every(8, s, &Shouter::shout);
    // kill after a while
    k.every(50, [](uint32_t t, uint32_t) {
            if (t) throw ("end");
            });
    CHECK_THROWS(k.run());
}
