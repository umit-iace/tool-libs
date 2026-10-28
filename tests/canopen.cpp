#include <doctest/doctest.h>
#include <comm/canopen.h>
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
