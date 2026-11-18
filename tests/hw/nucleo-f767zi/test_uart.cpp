/** @file test_uart.cpp
 *
 * UART::HW on the VCP (USART3), the rest is tested host driven by run.py
 *
 * Copyright (c) 2026 IACE
 */
#include <cstring>
#include <sys/uart.h>

#include "hwtest.h"

/* the HAL refusing to start a transmission (busy) is counted and retried
 * from the main loop, it used to end in a breakpoint (HardFault without
 * debugger)
 */
void test_uart_retry() {
    auto &u = *board::vcp;
    hwt::flush();
    REQUIRE(hwt::waitFor(100, [&]{ hwt::barrier(); return u.tx.q.empty() && !u.tx.active; }));
    uint32_t errors = u.tx.errors;
    {
        CriticalSection cs;
        u.handle.gState = HAL_UART_STATE_BUSY_TX; // pretend the HAL is busy
    }
    const char line[] = "HWT INFO sent after the HAL refused once\n";
    u.push(Buffer<uint8_t>((const uint8_t *)line, strlen(line)));
    hwt::barrier();
    CHECK_EQ(u.tx.errors, errors + 1);
    CHECK(!u.tx.active);
    {
        CriticalSection cs;
        u.handle.gState = HAL_UART_STATE_READY;
    }
    // HW::service (every ms) starts it again
    CHECK(hwt::waitFor(20, [&]{ hwt::barrier(); return u.tx.q.empty(); }));
}
