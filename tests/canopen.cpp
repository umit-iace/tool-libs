#include <doctest/doctest.h>
#include <comm/canopen.h>
#include <type_traits>
void Kernel::idle() { }
Kernel k;

using namespace CAN;
struct FakeCAN : CAN::CAN {
    Queue<Message> sent{50};
    bool full() override { return sent.full(); }
    void push(Message &&m) override { sent.push(std::move(m)); }
    bool empty() override { return true; }
    Message pop() override { return {}; }
};

struct Dev : Open::Device {
    Dev(Open::Dispatch &d) : Device(d, 5) { }
    void callback(Open::SDO) override { }
    void callback(Open::TPDO) override { }
};

TEST_CASE("tool-libs: canopen: SDO with high subindex") {
    Open::SDO s{.data=0x11223344, .ix=0x1600, .sub=0x80, .cmd=0x23, .nodeID=1};
    CHECK(s.toMessage().data == 0x1122334480160023);
    auto back = Open::SDO::fromMessage(s.toMessage());
    CHECK(back.sub == 0x80);
    CHECK(back.data == 0x11223344);
}

TEST_CASE("tool-libs: canopen: dispatch log level") {
    FakeCAN can;
    Open::Dispatch none{can};
    CHECK(none.log == Open::Dispatch::NONE);
    Open::Dispatch info{can, Open::Dispatch::INFO};
    CHECK(info.log == Open::Dispatch::INFO);
}

TEST_CASE("tool-libs: canopen: SDOs are sent in order") {
    FakeCAN can;
    Open::Dispatch d{can};
    Dev dev{d};
    dev.w8(0x2000, 0, 1);
    dev.w8(0x2000, 0, 2);
    dev.w8(0x2000, 0, 3);
    // no response in time
    for (int i = 0; i < 3; ++i) k.tick(1);
    dev.w8(0x2000, 0, 4);
    REQUIRE(can.sent.size() == 2);
    CHECK(Open::SDO::fromMessage(can.sent.pop()).data == 1);
    CHECK(Open::SDO::fromMessage(can.sent.pop()).data == 2);
}

static void tickrun(int ms) {
    for (int i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

TEST_CASE("tool-libs: canopen: lost SDO response doesn't stall the queue") {
    FakeCAN can;
    Open::Dispatch d{can};
    Dev dev{d};
    dev.w8(0x2000, 0, 1);
    dev.w8(0x2000, 0, 2);
    REQUIRE(can.sent.size() == 1);
    // no response and no further request: the kernel keeps it going
    tickrun(5);
    CHECK(can.sent.size() == 2);
}

TEST_CASE("tool-libs: canopen: devices unregister when they die") {
    FakeCAN can;
    Open::Dispatch d{can};
    size_t before = k.list.len;
    {
        Dev dev{d};
        CHECK(d.ids[5] == &dev);
        CHECK(k.list.len == before + 1);
    }
    CHECK(d.ids[5] == nullptr);
    CHECK(k.list.len == before);
    tickrun(3); // nothing dangling gets called
}

TEST_CASE("tool-libs: canopen: kernel time is atomic") {
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(k.time)>,
            std::atomic<uint32_t>>);
    uint32_t t = k.time;
    k.tick(1);
    CHECK(k.time == t + 1);
}

TEST_CASE("tool-libs: canopen: PDO mapping of high indices") {
    FakeCAN can;
    Open::Dispatch d{can};
    struct PDODev : Dev {
        using Dev::Dev;
        Open::TPDO pdo{
            .N = 1,
            .type = Open::PDOType::SYNC,
            .map = {{.ix = 0x9001, .sub = 2, .len = 32}},
        };
    } dev{d};
    dev.enablePDO(dev.pdo);
    tickrun(30); // drain the SDO queue, nobody responds
    bool found = false;
    while (!can.sent.empty()) {
        auto s = Open::SDO::fromMessage(can.sent.pop());
        if (s.ix == 0x1a00 && s.sub == 1) {
            CHECK(s.data == 0x90010220);
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("tool-libs: canopen: PDO with 64 mapped bits") {
    Open::TPDO t{.N = 1, .type = Open::PDOType::SYNC,
        .map = {{.ix = 1, .sub = 0, .len = 32}, {.ix = 2, .sub = 0, .len = 32}}};
    t.receive(0xfffffffe00000005ull);
    CHECK(t.map[0].data == 5);
    CHECK(t.map[1].data == -2);
    Open::RPDO r{.N = 1, .type = Open::PDOType::SYNC,
        .map = {{.ix = 1, .sub = 0, .len = 32, .data = -1}, {.ix = 2, .sub = 0, .len = 32, .data = 7}}};
    auto m = r.toMessage();
    CHECK(m.data == 0x00000007ffffffffull);
    CHECK((int)m.opts.dlc == 8);
}

#include <dev/mcDSA.h>
TEST_CASE("tool-libs: canopen: mcDSA negative measurements") {
    FakeCAN can;
    Open::Dispatch d{can};
    mcDSA m{d, 9};
    m.callback(Open::SDO{.data = (uint32_t)-1500, .ix = 0x3a04, .sub = 1});
    CHECK(m.meas.speed == -1500);
}
