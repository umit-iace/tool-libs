/** @file test_can.cpp
 *
 * CAN::HW on CAN1: RX PD0, TX PD1 (CN9), no transceiver needed
 *
 * the bit timing is checked as configured by the driver, the traffic
 * tests run in silent loopback mode
 *
 * Copyright (c) 2026 IACE
 */
#include <sys/can.h>

#include "hwtest.h"

namespace {
CAN::HW &can() {
    // pull-up on RX: the controller waits for a recessive bus to start
    static CAN::HW c({
        .can = CAN1,
        .rx = AFIO(GPIO_PIN_0, GPIOD, GPIO_AF9_CAN1, GPIO_PULLUP),
        .tx = AFIO(GPIO_PIN_1, GPIOD, GPIO_AF9_CAN1),
        .kbaud = 500,
    });
    board::can1 = &c;
    return c;
}

void loopback(CAN::HW &c) {
    if (c.handle.Init.Mode == CAN_MODE_SILENT_LOOPBACK) return;
    HAL_CAN_Stop(&c.handle);
    c.handle.Init.Mode = CAN_MODE_SILENT_LOOPBACK;
    HAL_CAN_Init(&c.handle);
    HAL_CAN_Start(&c.handle);
}

CAN::Message message(uint32_t i) {
    bool ext = i & 1;
    uint8_t dlc = i % 9;
    return {
        .data = 0x0102030405060708ull * (i + 1),
        .id = ext ? 0x1000000 + i : 0x100 + i,
        .opts = {.rtr = 0, .ide = ext, .dlc = dlc},
    };
}
/** data bytes beyond dlc aren't transmitted */
uint64_t payload(const CAN::Message &m) {
    if (m.opts.dlc >= 8) return m.data;
    return m.data & ((1ull << (8 * m.opts.dlc)) - 1);
}

bool txidle(CAN::HW &c) {
    hwt::barrier();
    return c.tx.q.empty() && HAL_CAN_GetTxMailboxesFreeLevel(&c.handle) == 3;
}
void drain(CAN::HW &c) {
    while (!c.empty()) c.pop();
}
}

void test_can_bittiming() {
    auto &c = can();
    uint32_t btr = CAN1->BTR;
    uint32_t brp = (btr & CAN_BTR_BRP) + 1;
    uint32_t ts1 = ((btr & CAN_BTR_TS1) >> CAN_BTR_TS1_Pos) + 1;
    uint32_t ts2 = ((btr & CAN_BTR_TS2) >> CAN_BTR_TS2_Pos) + 1;
    uint32_t tq = 1 + ts1 + ts2;
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    CHECK_EQ(pclk % (brp * tq), 0);
    CHECK_EQ(pclk / (brp * tq), 500000);
    uint32_t sample = 1000 * (1 + ts1) / tq;
    CHECK(sample >= 800 && sample <= 900);
    CHECK_EQ(c.handle.Init.Prescaler, brp);
    hwt::info("pclk1 %lu Hz, prescaler %lu, %lu tq, sample point %lu.%lu%%",
            pclk, brp, tq, sample / 10, sample % 10);
}

void test_can_loopback() {
    auto &c = can();
    loopback(c);
    drain(c);
    constexpr uint32_t N = 25; // fits the rx queue
    for (uint32_t i = 0; i < N; ++i) {
        while (c.full()) hwt::barrier();
        c.push(message(i));
    }
    uint32_t seen[N]{}, wrong = 0, got = 0;
    hwt::waitFor(100, [&]{
        while (!c.empty()) {
            auto m = c.pop();
            ++got;
            uint32_t i = m.opts.ide ? m.id - 0x1000000 : m.id - 0x100;
            if (i >= N) {
                ++wrong;
                continue;
            }
            auto want = message(i);
            if (m.opts.ide != want.opts.ide || m.opts.dlc != want.opts.dlc
                    || m.opts.rtr || m.data != payload(want)) {
                if (++wrong <= 5) {
                    hwt::info("msg %lu: ide %d dlc %d rtr %d data %08lx%08lx, "
                            "want ide %d dlc %d data %08lx%08lx", i,
                            m.opts.ide, m.opts.dlc, m.opts.rtr,
                            (uint32_t)(m.data >> 32), (uint32_t)m.data,
                            want.opts.ide, want.opts.dlc,
                            (uint32_t)(payload(want) >> 32),
                            (uint32_t)payload(want));
                }
            }
            ++seen[i];
        }
        return got >= N;
    });
    CHECK_EQ(got, N);
    CHECK_EQ(wrong, 0);
    int missing = 0;
    for (auto s : seen) missing += s != 1;
    CHECK_EQ(missing, 0);
    CHECK(txidle(c));
}

/* more than fits the rx queue: excess gets dropped in the interrupt */
void test_can_rx_overflow() {
    auto &c = can();
    loopback(c);
    drain(c);
    for (uint32_t i = 0; i < 100; ++i) {
        while (c.full()) hwt::barrier();
        c.push(message(i));
    }
    CHECK(hwt::waitFor(100, [&]{ return txidle(c); }));
    hwt::sleep(2);
    uint32_t got = 0;
    while (!c.empty()) {
        c.pop();
        ++got;
    }
    CHECK_EQ(got, 30);

    // still receiving afterwards
    for (uint32_t i = 0; i < 5; ++i) c.push(message(i));
    got = 0;
    CHECK(hwt::waitFor(20, [&]{
        while (!c.empty()) {
            c.pop();
            ++got;
        }
        return got == 5;
    }));
}
