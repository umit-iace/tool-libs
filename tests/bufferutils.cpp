#include <doctest/doctest.h>
#include <utils/queue.h>
#include <comm/bufferutils.h>

struct Count : Source<Buffer<uint8_t>> {
    int n = 0;
    bool empty() override { return n >= 50; }
    Buffer<uint8_t> pop() override { return Buffer<uint8_t>{(uint8_t)n++}; }
};

TEST_CASE("tool-libs: bufferutils: SplitPull with one side not reading") {
    Count src;
    SplitPull sp{src};
    int got = 0;
    while (!sp.a.empty()) {
        CHECK(sp.a.pop()[0] == got++);
    }
    CHECK(got == 50);
    CHECK(sp.b.full());
}
