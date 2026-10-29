/** @file main.cpp
 *
 * tool-libs hardware test runner for the NUCLEO-F767ZI
 *
 * waits for a 'g' on the ST-Link VCP, runs all on-target tests, then
 * echoes Min frames for the host driven UART test (see run.py)
 *
 * Copyright (c) 2026 IACE
 */
#include <cstdarg>
#include <cstdio>
#include <sys/uart.h>
#include <comm/min.h>

#include "board.h"
#include "hwtest.h"

Kernel k;

void test_critical_nesting();
void test_critical_blocks_irq();
void test_kernel_recurring();
void test_scheduler_isr_stress();
void test_malloc_isr_stress();
void test_logger();
void test_printf_float();
void test_pwm_registers();
void test_pwm_output();
void test_spi_until();
void test_spi_transfers();
void test_spi_loopback_data();
void test_spi_timeout();
void test_spi_queue_stress();
void test_i2c_nack();
void test_i2c_scan();
void test_can_bittiming();
void test_can_loopback();
void test_can_rx_overflow();
void test_i2c_timeout();
void test_adc_dcache();
void test_kernel_early();
void test_kernel_overrun();
void test_logger_blocking();

static const struct {
    const char *name;
    void (*fn)();
} tests[] = {
    {"critical_nesting", test_critical_nesting},
    {"critical_blocks_irq", test_critical_blocks_irq},
    {"kernel_recurring", test_kernel_recurring},
    {"kernel_early", test_kernel_early},
    {"kernel_overrun", test_kernel_overrun},
    {"scheduler_isr_stress", test_scheduler_isr_stress},
    {"malloc_isr_stress", test_malloc_isr_stress},
    {"logger", test_logger},
    {"printf_float", test_printf_float},
    {"logger_blocking", test_logger_blocking},
    {"pwm_registers", test_pwm_registers},
    {"pwm_output", test_pwm_output},
    {"spi_until", test_spi_until},
    {"spi_transfers", test_spi_transfers},
    {"spi_loopback_data", test_spi_loopback_data},
    {"spi_timeout", test_spi_timeout},
    {"spi_queue_stress", test_spi_queue_stress},
    {"i2c_nack", test_i2c_nack},
    {"i2c_scan", test_i2c_scan},
    {"i2c_timeout", test_i2c_timeout},
    {"can_bittiming", test_can_bittiming},
    {"can_loopback", test_can_loopback},
    {"can_rx_overflow", test_can_rx_overflow},
    {"adc_dcache", test_adc_dcache},
};

namespace hwt {
static int nfail;
static const char *skipped;

static void vprint(const char *fmt, va_list args) {
    va_list probe;
    va_copy(probe, args);
    int n = vsnprintf(nullptr, 0, fmt, probe);
    va_end(probe);
    if (n <= 0 || !board::vcp) return;
    Buffer<uint8_t> b = n + 1;
    vsnprintf((char *)b.buf, b.size, fmt, args);
    b.len = n;
    while (board::vcp->full()) barrier();
    board::vcp->push(std::move(b));
}
void print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint(fmt, args);
    va_end(args);
}
void flush() {
    auto &tx = board::vcp->tx;
    while (!tx.q.empty() || tx.active) barrier();
}
void fail(const char *file, int line, const char *expr) {
    ++nfail;
    print("HWT FAIL %s:%d: %s\n", file, line, expr);
}
void failv(const char *file, int line, const char *expr, long a, long b) {
    ++nfail;
    print("HWT FAIL %s:%d: %s (%ld != %ld)\n", file, line, expr, a, b);
}
void warn(const char *fmt, ...) {
    print("HWT WARN ");
    va_list args;
    va_start(args, fmt);
    vprint(fmt, args);
    va_end(args);
    print("\n");
}
void info(const char *fmt, ...) {
    print("HWT INFO ");
    va_list args;
    va_start(args, fmt);
    vprint(fmt, args);
    va_end(args);
    print("\n");
}
void skip(const char *why) {
    skipped = why;
}
int failures() {
    return nfail;
}
}

/** why stm/uart.cpp aborted transfers: HAL rx errors vs. its tx deadline */
static struct {
    uint32_t errors, codes, timeouts;
} uartAborts;
extern "C" {
HAL_StatusTypeDef __real_HAL_UART_AbortReceive(UART_HandleTypeDef *h);
HAL_StatusTypeDef __real_HAL_UART_AbortTransmit(UART_HandleTypeDef *h);
HAL_StatusTypeDef __wrap_HAL_UART_AbortReceive(UART_HandleTypeDef *h) {
    ++uartAborts.errors;
    uartAborts.codes |= h->ErrorCode;
    return __real_HAL_UART_AbortReceive(h);
}
HAL_StatusTypeDef __wrap_HAL_UART_AbortTransmit(UART_HandleTypeDef *h) {
    ++uartAborts.timeouts;
    return __real_HAL_UART_AbortTransmit(h);
}
}

static void waitForStart(UART::HW &uart) {
    uint32_t last = uwTick - 1000;
    while (true) {
        while (!uart.empty()) {
            for (auto c : uart.pop()) {
                if (c == 'g') return;
            }
        }
        if (uwTick - last >= 1000) {
            last = uwTick;
            hwt::print("HWT READY tool-libs hwtest NUCLEO-F767ZI, send 'g' to start\n");
        }
    }
}

/** host driven UART & Min test
 *
 * - id 0x3e: count only
 * - id 0x3f: end, reply with [echoed, counted, counted bytes] as uint32
 * - all others: echo back unchanged
 */
static void echo(UART::HW &uart) {
    while (!uart.empty()) uart.pop();
    Min min{.in = uart, .out = uart};
    uint32_t echoed = 0, counted = 0, bytes = 0;
    uartAborts = {};
    hwt::print("HWT ECHO START\n");
    hwt::flush();
    uint32_t last = uwTick;
    while (uwTick - last < 20000) {
        if (min.in.empty()) continue;
        Frame f = min.in.pop();
        last = uwTick;
        if (f.id == 0x3f) {
            Frame r{0x3f};
            r.pack(echoed).pack(counted).pack(bytes);
            min.out.push(std::move(r));
            hwt::flush();
            hwt::print("HWT ECHO END echoed=%lu counted=%lu bytes=%lu "
                    "aborts: errors=%lu (code 0x%lx) tx_timeouts=%lu\n",
                    echoed, counted, bytes, uartAborts.errors,
                    uartAborts.codes, uartAborts.timeouts);
            return;
        } else if (f.id == 0x3e) {
            ++counted;
            bytes += f.b.len;
        } else {
            ++echoed;
            while (uart.full()) hwt::barrier();
            min.out.push(std::move(f));
        }
    }
    hwt::print("HWT ECHO TIMEOUT echoed=%lu counted=%lu "
            "aborts: errors=%lu (code 0x%lx) tx_timeouts=%lu\n",
            echoed, counted, uartAborts.errors, uartAborts.codes,
            uartAborts.timeouts);
}

/** host driven UART overrun test
 *
 * the host sends a stream of bytes, interrupts are blocked for 3ms every
 * 10ms meanwhile: overruns. only the bytes arriving while blocked may get
 * lost, not what was received before the error
 */
static void overrun(UART::HW &uart) {
    while (!uart.empty()) uart.pop();
    hwt::print("HWT OVR START\n");
    hwt::flush();
    uartAborts = {};
    uint32_t bytes = 0, blocks = 0;
    uint32_t start = uwTick, last = uwTick, next = 0;
    while (uwTick - start < 10000) {
        while (!uart.empty()) {
            bytes += uart.pop().len;
            last = uwTick;
        }
        if (bytes && uwTick - last > 300) break; // host is done
        if (bytes && (int32_t)(uwTick - next) >= 0) {
            {
                CriticalSection cs;
                board::busy_us(3000);
            }
            ++blocks;
            next = uwTick + 10;
        }
    }
    hwt::print("HWT OVR END bytes=%lu errors=%lu code=0x%lx blocks=%lu\n",
            bytes, uartAborts.errors, uartAborts.codes, blocks);
}

int main() {
    board::init();
    UART::HW uart(UART::HW::Default{
        .uart = USART3,
        .rx = AFIO(GPIO_PIN_9, GPIOD, GPIO_AF7_USART3, GPIO_PULLUP),
        .tx = AFIO(GPIO_PIN_8, GPIOD, GPIO_AF7_USART3),
        .baudrate = HWTEST_BAUD,
    });
    board::vcp = &uart;

    waitForStart(uart);
    hwt::print("HWT START sysclk=%lu pclk1=%lu pclk2=%lu tests=%u\n",
            HAL_RCC_GetSysClockFreq(), HAL_RCC_GetPCLK1Freq(),
            HAL_RCC_GetPCLK2Freq(), sizeof(tests) / sizeof(*tests));

    int pass = 0, fail = 0, skip = 0;
    for (auto &t : tests) {
        hwt::print("HWT BEGIN %s\n", t.name);
        hwt::flush();
        hwt::nfail = 0;
        hwt::skipped = nullptr;
        uint32_t start = uwTick;
        t.fn();
        uint32_t ms = uwTick - start;
        const char *res = "PASS";
        if (hwt::nfail) {
            res = "FAIL";
            ++fail;
        } else if (hwt::skipped) {
            hwt::info("skipped: %s", hwt::skipped);
            res = "SKIP";
            ++skip;
        } else {
            ++pass;
        }
        hwt::print("HWT END %s %s %lu\n", t.name, res, ms);
    }
    hwt::print("HWT RESULT pass=%d fail=%d skip=%d\n", pass, fail, skip);

    echo(uart);
    overrun(uart);
    hwt::print("HWT DONE\n");
    hwt::flush();
    while (true) {
        k.Scheduler::run();
        k.idle();
    }
}
