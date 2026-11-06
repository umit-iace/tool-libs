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
    /** true if running in an interrupt handler, e.g. no heap allocations */
    static bool inInterrupt() {
        uint32_t ipsr;
        asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
        return ipsr != 0;
    }
    /** true if running in an interrupt handler or with interrupts disabled,
     * i.e. nothing else can make progress while waiting here */
    static bool blocking() {
        uint32_t ipsr, primask;
        asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
        asm volatile ("mrs %0, primask" : "=r"(primask));
        return ipsr != 0 || (primask & 1);
    }
#else
    static uint32_t enter() { return 0; }
    static void leave(uint32_t) { }
    static bool inInterrupt() { return false; }
    /** single threaded: waiting never lets anything else make progress */
    static bool blocking() { return true; }
#endif
};
