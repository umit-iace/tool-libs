/** @file hwtest.h
 *
 * minimal on-target test framework, reports over the ST-Link VCP
 *
 * every report line starts with "HWT ", run.py parses those:
 * ```
 * HWT BEGIN <name>
 * HWT FAIL <file>:<line>: <expression> [values]
 * HWT WARN <text>          (reported, but not counted as failure)
 * HWT INFO <text>
 * HWT END <name> PASS|FAIL|SKIP <ms>
 * ```
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <cstdint>
#include <core/kern.h>
#include "board.h"

namespace hwt {
/** keep the compiler from caching memory shared with interrupts */
inline void barrier() { asm volatile("" ::: "memory"); }
void print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/** wait until everything printed so far left the UART */
void flush();
void fail(const char *file, int line, const char *expr);
void failv(const char *file, int line, const char *expr, long a, long b);
void warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/** mark running test as skipped, return from the test afterwards */
void skip(const char *why);
/** number of failures in the running test */
int failures();

/** run the kernel's scheduled calls until `done()` or `ms` passed
 *
 * returns whether `done()` became true
 */
template<typename F>
bool waitFor(uint32_t ms, F done) {
    uint32_t start = uwTick;
    while (!done()) {
        if (uwTick - start > ms) return false;
        k.Scheduler::run();
    }
    return true;
}
/** run the kernel's scheduled calls for `ms` */
inline void sleep(uint32_t ms) {
    waitFor(ms, []{ return false; });
}
}

#define CHECK(e) do { \
    if (!(e)) hwt::fail(__FILE__, __LINE__, #e); \
} while (0)
#define CHECK_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) hwt::failv(__FILE__, __LINE__, #a " == " #b, _a, _b); \
} while (0)
#define REQUIRE(e) do { \
    if (!(e)) { hwt::fail(__FILE__, __LINE__, #e); return; } \
} while (0)
