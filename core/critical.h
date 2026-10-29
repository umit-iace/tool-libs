/** @file critical.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <cstdint>

/** RAII guard keeping interrupts disabled while alive
 *
 * use this to protect data shared between interrupt handlers and the main
 * loop, e.g. Queues. can be nested, the previous interrupt state is
 * restored on destruction.
 * does nothing when not running on a Cortex-M (i.e. in simulation).
 * ```
 * {
 *     CriticalSection cs;
 *     q.push(std::move(val));
 * }
 * ```
 */
class CriticalSection {
    uint32_t primask;
public:
    CriticalSection() : primask(enter()) { }
    ~CriticalSection() { leave(primask); }
    CriticalSection(const CriticalSection &)=delete;
    CriticalSection& operator=(const CriticalSection &)=delete;

#if defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M'
    /** disable interrupts, return previous state */
    static uint32_t enter() {
        uint32_t primask;
        asm volatile ("mrs %0, primask\n\tcpsid i" : "=r"(primask) :: "memory");
        return primask;
    }
    /** restore interrupt state returned by enter() */
    static void leave(uint32_t primask) {
        asm volatile ("msr primask, %0" :: "r"(primask) : "memory");
    }
#else
    static uint32_t enter() { return 0; }
    static void leave(uint32_t) { }
#endif
};
