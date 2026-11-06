/** @file test_spi.cpp
 *
 * SPI::HW on SPI1: SCK PA5 (D13), MISO PA6 (D12), MOSI PB5 (D22),
 * chip selects PD14 (D10) & PD15 (D9)
 *
 * optional jumper MOSI PB5 <-> MISO PA6 for spi_loopback_data
 *
 * Copyright (c) 2026 IACE
 */
#include <cstring>
#include <sys/spi.h>

#include "hwtest.h"

namespace SPI {
Deadline until(SPI_HandleTypeDef *handle, size_t datasize, uint32_t now);
}

namespace {
SPI::HW &bus() {
    static SPI::HW spi({
        .spi = SPI1,
        .miso = AFIO(GPIO_PIN_6, GPIOA, GPIO_AF5_SPI1, GPIO_PULLDOWN),
        .mosi = AFIO(GPIO_PIN_5, GPIOB, GPIO_AF5_SPI1),
        .sck = AFIO(GPIO_PIN_5, GPIOA, GPIO_AF5_SPI1),
        .prescaler = SPI_BAUDRATEPRESCALER_32, // 108MHz / 32
    });
    board::spi1 = &spi;
    return spi;
}

/** records what got completed, the callback runs in the main loop */
struct Dev : SPI::Device {
    uint32_t calls{}, bad{}, seq{}, inIsr{};
    uint32_t lastlen{};
    uint8_t rx[64]{};
    bool sequence{}; ///< expect lengths 1..32, 1..32, ...
    Dev(SPI::HW &bus, uint32_t cspin, SPI::Mode m, SPI::FirstBit fb)
        : Device(bus, DIO(cspin, GPIOD), {m, fb}) { }
    void callback(const SPI::Request &rq) override {
        if (sequence) {
            if (rq.data.len != seq % 32 + 1) bad = bad + 1;
            seq = seq + 1;
        }
        lastlen = rq.data.len;
        memcpy(rx, rq.data.buf, rq.data.len < sizeof rx ? rq.data.len : sizeof rx);
        calls = calls + 1;
        if (CriticalSection::inInterrupt()) inIsr = inIsr + 1;
    }
    void reset() {
        calls = bad = seq = lastlen = inIsr = 0;
        sequence = false;
    }
};
Dev &dev0() {
    static Dev d(bus(), GPIO_PIN_14, SPI::Mode::M0, SPI::FirstBit::MSB);
    return d;
}
Dev &dev3() {
    static Dev d(bus(), GPIO_PIN_15, SPI::Mode::M3, SPI::FirstBit::LSB);
    return d;
}

SPI::Request request(Dev &d, size_t len, SPI::Request::Direction dir, uint8_t start=1) {
    Buffer<uint8_t> b(len);
    for (size_t i = 0; i < len; ++i) b.buf[i] = start + i;
    if (dir != SPI::Request::MISO) b.len = len;
    return {.dev = &d, .data = std::move(b), .dir = dir};
}

/** nothing transferring, all callbacks delivered */
bool idle(SPI::HW &spi) {
    hwt::barrier();
    return !spi.active.xfer && spi.q.empty() && spi.completed.done.empty();
}
}

void test_spi_until() {
    SPI_HandleTypeDef h{};
    // the slower APB (54MHz) is assumed, prescaler 256: 210 bit/ms
    h.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    CHECK_EQ(SPI::until(&h, 1000, 100).when, 100 + 8000 / 210 + 2);
    // prescaler 2: 27000 bit/ms, short transfers get the 2ms margin
    h.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    CHECK_EQ(SPI::until(&h, 1, 100).when, 102);
}

void test_spi_transfers() {
    auto &spi = bus();
    auto &a = dev0(), &b = dev3();
    a.reset();
    b.reset();

    spi.push(request(a, 4, SPI::Request::MOSI));
    spi.push(request(b, 7, SPI::Request::MISO));
    spi.push(request(a, 9, SPI::Request::BOTH));
    spi.push(request(b, 12, SPI::Request::BOTH));
    CHECK(hwt::waitFor(20, [&]{ return idle(spi); }));
    CHECK_EQ(a.calls, 2);
    CHECK_EQ(b.calls, 2);
    CHECK_EQ(a.inIsr + b.inIsr, 0);
    CHECK_EQ(a.lastlen, 9);
    CHECK_EQ(b.lastlen, 12);
    // chip selects released
    CHECK(GPIOD->ODR & GPIO_PIN_14);
    CHECK(GPIOD->ODR & GPIO_PIN_15);
    // mode switch to M3/LSB happened & stayed
    CHECK(spi.handle.Init.CLKPolarity == SPI_POLARITY_HIGH);
    CHECK(spi.handle.Init.FirstBit == SPI_FIRSTBIT_LSB);

    // a long transfer finishes well within its deadline
    uint32_t start = uwTick;
    spi.push(request(a, 2000, SPI::Request::MOSI));
    uint32_t allowed = spi.deadline.when - start;
    CHECK(hwt::waitFor(100, [&]{ return idle(spi); }));
    uint32_t took = uwTick - start;
    CHECK(took < allowed);
    CHECK_EQ(a.lastlen, 2000);
    hwt::info("2000 bytes took %lu ms, deadline allowed %lu ms", took, allowed);
}

void test_spi_loopback_data() {
    auto &spi = bus();
    auto &a = dev0();
    a.reset();
    spi.push(request(a, 64, SPI::Request::BOTH, 0x11));
    CHECK(hwt::waitFor(20, [&]{ return idle(spi); }));
    REQUIRE(a.calls == 1);
    bool echoed = true, zero = true;
    for (int i = 0; i < 64; ++i) {
        if (a.rx[i] != (uint8_t)(0x11 + i)) echoed = false;
        if (a.rx[i] != 0) zero = false;
    }
    if (zero) return hwt::skip("no jumper MOSI PB5 <-> MISO PA6");
    CHECK(echoed);
}

/* a transfer whose interrupt never comes must be given up at the next
 * poll, and must not block the bus for later requests
 */
void test_spi_timeout() {
    auto &spi = bus();
    auto &a = dev0();
    a.reset();
    REQUIRE(idle(spi));

    HAL_NVIC_DisableIRQ(SPI1_IRQn);
    spi.push(request(a, 16, SPI::Request::MOSI));
    CHECK(spi.active.xfer);
    uint32_t when = spi.deadline.when;
    while (uwTick <= when + 1) ;

    // next push polls the bus: drops the stuck request, starts this one
    uint32_t start = board::cycles();
    spi.push(request(a, 5, SPI::Request::MOSI));
    uint32_t us = (board::cycles() - start) / (SystemCoreClock / 1000000);
    CHECK_EQ(spi.q.size(), 1);
    CHECK(spi.active.xfer);
    CHECK_EQ(a.calls, 0);
    // push runs in a CriticalSection, giving up must not wait for interrupts
    CHECK(us < 100);
    hwt::info("giving up the stuck transfer took %lu us", us);

    HAL_NVIC_EnableIRQ(SPI1_IRQn);
    CHECK(hwt::waitFor(20, [&]{ return idle(spi); }));
    // only the second one got through
    CHECK_EQ(a.calls, 1);
    CHECK_EQ(a.lastlen, 5);
    CHECK(GPIOD->ODR & GPIO_PIN_14);

    // and the bus keeps working
    spi.push(request(a, 3, SPI::Request::BOTH));
    CHECK(hwt::waitFor(20, [&]{ return idle(spi); }));
    CHECK_EQ(a.calls, 2);
}

/* push from the main loop while completions run in the interrupt */
void test_spi_queue_stress() {
    auto &spi = bus();
    auto &a = dev0(), &b = dev3();
    a.reset();
    b.reset();
    a.sequence = true;
    constexpr uint32_t N = 2000;
    for (uint32_t i = 0; i < N; ++i) {
        while (spi.full()) k.Scheduler::run();
        auto dir = i % 3 == 0 ? SPI::Request::MOSI : SPI::Request::BOTH;
        spi.push(request(a, i % 32 + 1, dir));
        // interleave the other device, forcing mode switches
        if (i % 50 == 0) {
            while (spi.full()) k.Scheduler::run();
            spi.push(request(b, 2, SPI::Request::BOTH));
        }
    }
    CHECK(hwt::waitFor(500, [&]{ return idle(spi); }));
    CHECK_EQ(a.calls, N);
    CHECK_EQ(a.bad, 0);
    CHECK_EQ(b.calls, N / 50);
    // the callbacks ran in the main loop, none got lost
    CHECK_EQ(a.inIsr + b.inIsr, 0);
    CHECK_EQ(spi.completed.lost, 0);
}
