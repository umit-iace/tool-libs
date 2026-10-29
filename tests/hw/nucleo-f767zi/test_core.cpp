/** @file test_core.cpp
 *
 * CriticalSection, Kernel & Scheduler, interrupt safe heap
 *
 * Copyright (c) 2026 IACE
 */
#include <cstdlib>
#include <malloc.h>
#include <core/critical.h>

#include "hwtest.h"

static uint32_t xorshift(uint32_t &s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

void test_critical_nesting() {
    CHECK_EQ(__get_PRIMASK(), 0);
    {
        CriticalSection a;
        CHECK_EQ(__get_PRIMASK(), 1);
        {
            CriticalSection b;
            CHECK_EQ(__get_PRIMASK(), 1);
        }
        // inner guard must not reenable interrupts
        CHECK_EQ(__get_PRIMASK(), 1);
    }
    CHECK_EQ(__get_PRIMASK(), 0);

    // already disabled interrupts stay disabled
    __disable_irq();
    {
        CriticalSection c;
    }
    CHECK_EQ(__get_PRIMASK(), 1);
    __enable_irq();
}

void test_critical_blocks_irq() {
    uint32_t before, after;
    {
        CriticalSection cs;
        before = uwTick;
        board::busy_us(3000);
        after = uwTick;
    }
    CHECK_EQ(after, before);
    // the pending SysTick has to fire right after leaving
    board::busy_us(10);
    CHECK(uwTick != before);
}

/* shared with the interrupt, synchronized through the CriticalSections
 * in Scheduler::run, or by stopping the interrupt first
 */
namespace kern {
bool active;
uint32_t count, last, bad;
}
void test_kernel_recurring() {
    using namespace kern;
    count = bad = 0;
    active = true;
    k.every(10, [](uint32_t t, uint32_t dt) {
        if (!active) return;
        if (dt != 10) ++bad;
        if (count && t - last != 10) ++bad;
        last = t;
        ++count;
    });
    uint32_t k0 = k.time, u0 = uwTick;
    hwt::sleep(500);
    uint32_t dk = k.time - k0, du = uwTick - u0;
    active = false;

    CHECK(dk + 1 >= du && du + 1 >= dk);
    CHECK(count >= 49 && count <= 51);
    CHECK_EQ(bad, 0);
    hwt::info("kernel time %lu ms, HAL time %lu ms, %lu calls", dk, du, count);
}

/* Scheduler fed from a 20kHz interrupt while the main loop keeps growing
 * the registry. Registry::add swaps the list under the running ISR.
 */
namespace stress {
struct Counter {
    uint32_t n, last, bad, added, dt;
    void call(uint32_t t, uint32_t d) {
        if (d != dt) ++bad;
        if (n && t - last != dt) ++bad;
        last = t;
        ++n;
    }
};
constexpr int N = 150;
Counter cnt[N];
Schedule::Recurring::Registry reg;
Scheduler sched;
uint32_t now, overflow;
void tick() {
    now = now + 1;
    if (!sched.schedule(now, reg)) overflow = overflow + 1;
}
}
void test_scheduler_isr_stress() {
    using namespace stress;
    uint32_t seed = 0x12345678;
    board::stress7(20000, tick);
    // a Recurring::Schedulable registered before `dt` passed gets aligned
    // to multiples of dt, which the spacing check can't handle
    while (now < 400) hwt::barrier();
    for (int i = 0; i < N; ++i) {
        cnt[i].dt = 200 + (i % 13) * 10;
        cnt[i].added = now;
        reg.every(cnt[i].dt, cnt[i], &Counter::call);
        // drain for a random while, so the next add hits the ISR somewhere else
        uint32_t start = board::cycles();
        uint32_t wait = (xorshift(seed) % 50) * (SystemCoreClock / 1000000);
        while (board::cycles() - start < wait) sched.run();
    }
    uint32_t start = uwTick;
    while (uwTick - start < 500) sched.run();
    board::stress7_stop();
    sched.run();

    CHECK_EQ(overflow, 0);
    CHECK_EQ(reg.list.len, N);
    int wrongcount = 0, wrongspacing = 0;
    for (auto &c : cnt) {
        // first call right after registering, then every dt
        uint32_t expected = (now - c.added - 1) / c.dt + 1;
        if (c.n + 1 < expected || c.n > expected + 1) ++wrongcount;
        if (c.bad) ++wrongspacing;
    }
    CHECK_EQ(wrongcount, 0);
    CHECK_EQ(wrongspacing, 0);
    hwt::info("%lu interrupt ticks, registry grew to %u entries",
            now, (unsigned)reg.list.size);
}

/* heap use from the main loop and a 50kHz interrupt at the same time,
 * like Buffers being (de)allocated in UART/CAN handlers
 */
namespace heap {
constexpr int ISR_SLOTS = 8;
uint8_t *isrp[ISR_SLOTS];
uint16_t isrsz[ISR_SLOTS];
uint32_t isrn, isrbad, isroom;
uint32_t isrseed = 0xdeadbeef;

void fill(uint8_t *p, size_t sz, uint8_t tag) {
    for (size_t i = 0; i < sz; ++i) p[i] = tag ^ (uint8_t)i;
}
bool verify(const uint8_t *p, size_t sz, uint8_t tag) {
    for (size_t i = 0; i < sz; ++i) {
        if (p[i] != (uint8_t)(tag ^ (uint8_t)i)) return false;
    }
    return true;
}
void isr() {
    int i = isrn % ISR_SLOTS;
    if (isrp[i]) {
        if (!verify(isrp[i], isrsz[i], 0x80 | i)) isrbad = isrbad + 1;
        free(isrp[i]);
    }
    uint16_t sz = 1 + xorshift(isrseed) % 256;
    isrp[i] = (uint8_t *)malloc(sz);
    isrsz[i] = sz;
    if (isrp[i]) {
        fill(isrp[i], sz, 0x80 | i);
    } else {
        isroom = isroom + 1;
    }
    isrn = isrn + 1;
}
}
void test_malloc_isr_stress() {
    using namespace heap;
    constexpr int SLOTS = 64;
    uint8_t *p[SLOTS]{};
    uint16_t sz[SLOTS]{};
    uint32_t seed = 0xcafe, ops = 0, bad = 0, oom = 0;

    free(malloc(1)); // let newlib set up its state first
    size_t baseline = mallinfo().uordblks;

    board::stress6(50000, isr);
    uint32_t start = uwTick;
    while (uwTick - start < 2000) {
        int i = xorshift(seed) % SLOTS;
        if (p[i]) {
            if (!verify(p[i], sz[i], i)) ++bad;
            free(p[i]);
            p[i] = nullptr;
        } else {
            sz[i] = 1 + xorshift(seed) % 1024;
            p[i] = (uint8_t *)malloc(sz[i]);
            if (p[i]) fill(p[i], sz[i], i);
            else ++oom;
        }
        ++ops;
    }
    board::stress6_stop();
    hwt::barrier();
    for (auto &q : isrp) {
        free(q);
        q = nullptr;
    }
    for (int i = 0; i < SLOTS; ++i) {
        if (p[i] && !verify(p[i], sz[i], i)) ++bad;
        free(p[i]);
    }

    CHECK_EQ(bad, 0);
    CHECK_EQ(isrbad, 0);
    CHECK_EQ(oom, 0);
    CHECK_EQ(isroom, 0);
    CHECK(isrn > 50000);
    CHECK_EQ(mallinfo().uordblks, baseline);
    hwt::info("%lu main loop & %lu interrupt heap operations", ops, isrn);
}
