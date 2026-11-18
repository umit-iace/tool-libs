/** linux CAN::HW without a CAN interface: frame conversion, reception
 * through a socketpair instead of a CAN socket
 */
#include <doctest/doctest.h>
#include <core/kern.h>
#include <sys/can.h>
#include <sys/socket.h>

Kernel k; // linux/hal.cpp needs one
using CAN::Message;

TEST_CASE("tool-libs: linux can: standard, extended & remote frames") {
    Message std{.data = 0x0807060504030201, .id = 0x123, .opts = {.rtr = 0, .ide = 0, .dlc = 3}};
    auto f = CAN::HW::frameFromMessage(Message{std});
    CHECK(f.can_id == 0x123);
    CHECK(f.len == 3);
    CHECK(f.data[2] == 3);
    CHECK(f.data[3] == 0);

    Message ext{.data = 0, .id = 0x1abcdef0, .opts = {.rtr = 0, .ide = 1, .dlc = 0}};
    f = CAN::HW::frameFromMessage(Message{ext});
    CHECK(f.can_id == (0x1abcdef0 | CAN_EFF_FLAG));
    auto back = CAN::HW::messageFromFrame(f);
    CHECK(back.id == 0x1abcdef0);
    CHECK((bool)back.opts.ide);

    Message rtr{.data = 0xff, .id = 0x7ff, .opts = {.rtr = 1, .ide = 0, .dlc = 2}};
    f = CAN::HW::frameFromMessage(Message{rtr});
    CHECK(f.can_id == (0x7ff | CAN_RTR_FLAG));
    back = CAN::HW::messageFromFrame(f);
    CHECK((bool)back.opts.rtr);
    CHECK((int)back.opts.dlc == 2);
    CHECK(back.data == 0); // remote frames carry no data
}

TEST_CASE("tool-libs: linux can: error frames are dropped, full queue isn't overrun") {
    CAN::HW can{"nonexistent-can"};
    int sv[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == 0);
    close(can.sock);
    can.sock = sv[0];

    struct can_frame err {};
    err.can_id = CAN_ERR_FLAG | 0x40; // CAN_ERR_BUSOFF
    err.len = 8;
    REQUIRE(write(sv[1], &err, sizeof err) == sizeof err);
    CHECK(can.empty());

    size_t capacity = can.rx.capacity();
    for (size_t i = 0; i < capacity + 5; ++i) {
        struct can_frame f {};
        f.can_id = 0x100 + i;
        f.len = 1;
        REQUIRE(write(sv[1], &f, sizeof f) == sizeof f);
    }
    for (size_t i = 0; i < capacity + 5; ++i) can.empty();
    CHECK(can.rx.size() == capacity);
    // the rest is still waiting in the socket
    size_t got = 0;
    while (!can.empty()) {
        CHECK(can.pop().id == 0x100 + got);
        ++got;
    }
    CHECK(got == capacity + 5);
    close(sv[1]);
}
