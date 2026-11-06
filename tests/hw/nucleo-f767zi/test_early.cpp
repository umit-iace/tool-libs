/** @file test_early.cpp
 *
 * a global object registering with the Kernel in its constructor, like a
 * sensor driver would. this file is linked before main.cpp, which defines
 * `Kernel k`: its globals are constructed before the Kernel.
 *
 * Copyright (c) 2026 IACE
 */
#include "hwtest.h"

namespace {
struct Early {
    uint32_t calls{};
    size_t listSizeBefore;
    Schedule::Schedulable *handle;
    Early() : listSizeBefore(k.list.size) {
        handle = k.every(10, *this, &Early::call);
    }
    void call(uint32_t, uint32_t) { calls = calls + 1; }
} early;
}

void test_kernel_early() {
    // the registration really happened before the Kernel's constructor
    CHECK_EQ(early.listSizeBefore, 0);
    REQUIRE(early.handle);
    uint32_t before = early.calls;
    hwt::sleep(100);
    hwt::barrier();
    uint32_t calls = early.calls - before;
    CHECK(calls >= 9 && calls <= 11);
    hwt::info("registered before the Kernel existed, called %lu times in 100 ms", calls);
    CHECK(k.remove(early.handle));
}
