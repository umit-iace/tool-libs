#include <doctest/doctest.h>
#include <core/experiment.h>
// time is advanced manually in this test
void Kernel::idle() { }
Kernel k;

static void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

struct Status {
    uint8_t version, state, flags, seq;
    uint16_t count;
    uint32_t crc, uptime;
    uint8_t syncSeq;
    uint16_t syncCount;
    uint32_t syncCrc;
};

static Status unpack(Frame f) {
    Status s{};
    s.version = f.unpack<uint8_t>();
    s.state = f.unpack<uint8_t>();
    s.flags = f.unpack<uint8_t>();
    s.seq = f.unpack<uint8_t>();
    s.count = f.unpack<uint16_t>();
    s.crc = f.unpack<uint32_t>();
    s.uptime = f.unpack<uint32_t>();
    s.syncSeq = f.unpack<uint8_t>();
    s.syncCount = f.unpack<uint16_t>();
    s.syncCrc = f.unpack<uint32_t>();
    CHECK(f.remaining() == 0);
    return s;
}

static Frame param(uint8_t id, std::initializer_list<uint8_t> payload) {
    Frame f{id};
    for (auto b: payload) f.pack(b);
    return f;
}

/** last frame in the queue, the older ones are dropped */
static Frame last(Queue<Frame> &q) {
    Frame f;
    while (!q.empty()) f = q.pop();
    return f;
}

TEST_CASE("tool-libs: experiment: status while idle and running") {
    FrameRegistry reg;
    Experiment e{&reg};
    Queue<Frame> out{50};
    e.statusFlags = 0b101;
    e.enableStatus(out, 100);

    step(250);
    CHECK(out.size() == 3); // at 0, 100 and 200 ms
    auto s = unpack(last(out));
    CHECK(s.version == 1);
    CHECK(s.state == 0);
    CHECK(s.flags == 0b101);
    CHECK(s.seq == 0);
    CHECK(s.uptime >= 200);

    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b01))); // start
    step(1);
    // sent at once with the start, not only with the next period
    REQUIRE(!out.empty());
    s = unpack(last(out));
    CHECK(s.state == 1);
    CHECK(s.seq == 1);
}

TEST_CASE("tool-libs: experiment: status carries the CRC of the frames before the start") {
    FrameRegistry reg;
    Experiment e{&reg};
    Queue<Frame> out{50};
    e.enableStatus(out, 100);

    // stop of an earlier run resets the count, heartbeats don't
    reg.handle(param(10, {9, 9}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b00)));
    reg.handle(param(20, {1, 2, 3}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b10)));
    reg.handle(param(21, {}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b01)));
    step(1);

    CRC32 crc;
    for (uint8_t b: {20, 3, 1, 2, 3, 21, 0}) crc.step(b);
    auto s = unpack(last(out));
    CHECK(s.count == 2);
    CHECK(s.crc == crc.finalize());
    // zlib.crc32(bytes([20, 3, 1, 2, 3, 21, 0])), as computed by pyWisp
    CHECK(s.crc == 0x2af3ac7bu);

    // handlers still get the complete frame
    static uint8_t got;
    reg.setHandler(30, [](Frame &f) { got = f.unpack<uint8_t>(); });
    reg.handle(param(30, {42}));
    CHECK(got == 42);
}

TEST_CASE("tool-libs: experiment: sync command during the run") {
    FrameRegistry reg;
    Experiment e{&reg};
    Queue<Frame> out{50};
    e.enableStatus(out, 100);
    e.setHeartbeatTimeout(100, out);

    reg.handle(param(20, {5}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b01))); // start
    step(50);
    last(out);

    // changed parameters, then a sync with id 7: status at once, heartbeat restarted
    reg.handle(param(20, {1, 2, 3}));
    reg.handle(param(21, {}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b110).pack<uint8_t>(7)));
    REQUIRE(!out.empty());
    auto s = unpack(last(out));
    CHECK(s.state == 1);
    CHECK(s.seq == 1); // start counter unchanged
    CHECK(s.syncSeq == 7);
    CHECK(s.syncCount == 2);
    CHECK(s.syncCrc == 0x2af3ac7bu);
    step(80);
    CHECK(e.state == e.RUN); // the sync counts as heartbeat

    // the next sync only covers the frames after the last one
    reg.handle(param(20, {1, 2, 3}));
    reg.handle(std::move(Frame{1}.pack<uint8_t>(0b110).pack<uint8_t>(8)));
    s = unpack(last(out));
    CHECK(s.syncSeq == 8);
    CHECK(s.syncCount == 1);
}
