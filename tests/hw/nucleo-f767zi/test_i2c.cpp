/** @file test_i2c.cpp
 *
 * I2C::HW on I2C1: SCL PB8 (D15), SDA PB9 (D14), internal pull-ups
 *
 * without anything connected all requests have to be NACKed and dropped,
 * sensors connected to the bus are listed by i2c_scan
 *
 * Copyright (c) 2026 IACE
 */
#include <sys/i2c.h>

#include "hwtest.h"

namespace {
I2C::HW &bus() {
    static I2C::HW i2c({
        .i2c = I2C1,
        .sda = AFIO(GPIO_PIN_9, GPIOB, GPIO_AF4_I2C1, GPIO_PULLUP,
                GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD),
        .scl = AFIO(GPIO_PIN_8, GPIOB, GPIO_AF4_I2C1, GPIO_PULLUP,
                GPIO_SPEED_FREQ_HIGH, GPIO_MODE_AF_OD),
        .baud = 400000,
        .address = 0,
    });
    board::i2c1 = &i2c;
    return i2c;
}

/** reserved address, nothing answers here */
constexpr uint8_t NOBODY = 0x7f;

struct Ghost : I2C::Device {
    uint32_t calls{};
    Ghost(I2C::HW &bus) : Device(bus, NOBODY) { }
    void callback(const I2C::Request &) override { calls = calls + 1; }
};

I2C::Request request(Ghost &g, decltype(I2C::Request::Type::type) type) {
    I2C::Request rq{.dev = &g, .data = Buffer<uint8_t>(6)};
    rq.opts.type = type;
    rq.mem = 0x0c;
    if (!rq.opts.read) {
        rq.data.append(0xaa).append(0x55);
    }
    return rq;
}

Ghost &ghost() {
    static Ghost g(bus());
    return g;
}

bool idle(I2C::HW &i2c) {
    hwt::barrier();
    return i2c.active == i2c.NONE && i2c.q.empty();
}

/** throw away requests nobody started, so the next test starts clean */
void unstick(I2C::HW &i2c) {
    if (hwt::waitFor(10, [&]{ return idle(i2c); })) return;
    CriticalSection cs;
    hwt::info("queue stuck with %u requests (active=%d), clearing",
            (unsigned)i2c.q.size(), (int)i2c.active);
    while (!i2c.q.empty()) i2c.q.pop();
}
}

void test_i2c_nack() {
    auto &i2c = bus();
    auto &g = ghost();
    g.calls = 0;
    if (!(GPIOB->IDR & GPIO_PIN_8) || !(GPIOB->IDR & GPIO_PIN_9)) {
        return hwt::skip("SCL/SDA held low, bus shorted or unpowered device");
    }

    for (auto type : {I2C::Request::Type::MEM_READ,
                      I2C::Request::Type::MEM_WRITE,
                      I2C::Request::Type::MASTER_READ,
                      I2C::Request::Type::MASTER_WRITE}) {
        uint32_t start = uwTick;
        i2c.push(request(g, type));
        // the NACK has to drop the request, there is no later poll that
        // would notice the deadline
        CHECK(hwt::waitFor(10, [&]{ return idle(i2c); }));
        CHECK(uwTick - start <= 2);
    }
    CHECK_EQ(g.calls, 0);

    // a whole queue of them, each NACK has to start the next request
    for (int i = 0; i < 20; ++i) {
        i2c.push(request(g, I2C::Request::Type::MEM_READ));
    }
    CHECK(hwt::waitFor(50, [&]{ return idle(i2c); }));
    CHECK_EQ(g.calls, 0);
    unstick(i2c);
}

void test_i2c_scan() {
    auto &i2c = bus();
    if (!idle(i2c)) return hwt::skip("bus busy");
    char found[128] = "";
    int n = 0;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        if (HAL_I2C_IsDeviceReady(&i2c.handle, addr << 1, 1, 2) == HAL_OK) {
            n += snprintf(found + n, sizeof found - n, " 0x%02x", addr);
            if (n >= (int)sizeof found) break;
        }
    }
    hwt::info("I2C devices found:%s", n ? found : " none");
    // the async driver must still work after the blocking HAL calls
    i2c.push(request(ghost(), I2C::Request::Type::MEM_READ));
    CHECK(hwt::waitFor(10, [&]{ return idle(i2c); }));
    unstick(i2c);
}
