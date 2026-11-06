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

struct Collect : Sink<Buffer<uint8_t>> {
    Buffer<uint8_t> last;
    bool full() override { return false; }
    void push(Buffer<uint8_t> &&b) override { last = std::move(b); }
};

TEST_CASE("tool-libs: bufferutils: Hexify of big buffers") {
    Collect out;
    Hexify h{out};
    Buffer<uint8_t> in(600);
    for (int i = 0; i < 600; ++i) in.append(i);
    h.push(std::move(in));
    REQUIRE(out.last.len == 1800);
    CHECK(out.last[1797] == '\\');
    CHECK(out.last[1798] == '5'); // 599 & 0xff = 0x57
    CHECK(out.last[1799] == '7');
}
