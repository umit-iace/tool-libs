/** @file test_logger.cpp
 *
 * Logger formatting with newlib-nano's printf
 *
 * Copyright (c) 2026 IACE
 */
#include <cstring>

#include "hwtest.h"

namespace {
struct Capture : Sink<Buffer<uint8_t>> {
    Buffer<uint8_t> last{1};
    int pushed{};
    bool full() override { return false; }
    void push(Buffer<uint8_t> &&b) override {
        last = std::move(b);
        ++pushed;
    }
    bool is(const char *s) {
        return last.len == strlen(s) && memcmp(last.buf, s, last.len) == 0;
    }
    bool endswith(const char *s) {
        size_t n = strlen(s);
        return last.len >= n && memcmp(last.buf + last.len - n, s, n) == 0;
    }
};
}

void test_logger() {
    Capture cap;
    Logger log{cap};
    log.print("%lu %d %s %x", 4000000000UL, -12, "abc", 0xbeef);
    CHECK(cap.is("4000000000 -12 abc beef"));

    log.info("ok");
    CHECK(cap.is("\u001b[32mok\u001b[0m"));

    // truncated, but the color still gets reset
    char longer[400];
    memset(longer, 'x', sizeof longer - 1);
    longer[sizeof longer - 1] = 0;
    log.warn("%s", longer);
    CHECK(cap.last.len < cap.last.size);
    CHECK(cap.endswith("\u001b[0m"));

    BufferedLogger blog{cap};
    cap.pushed = 0;
    for (int i = 0; i < 100; ++i) blog.put("%03d,", i);
    blog.flush();
    // 400 chars don't fit into one 256 char buffer
    CHECK_EQ(cap.pushed, 2);
    CHECK(cap.endswith("098,099,"));
}

void test_printf_float() {
    // e.g. dev/ODrive.h formats floats, newlib-nano drops them silently
    // unless linked with -u _printf_float
    char buf[32];
    snprintf(buf, sizeof buf, "%.2f", 1.5);
    if (strcmp(buf, "1.50") != 0) {
        hwt::warn("printf has no float support, link with -u _printf_float");
    }
    CHECK(strcmp(buf, "1.50") == 0);
}
