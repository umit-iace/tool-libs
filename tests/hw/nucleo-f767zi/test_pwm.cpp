/** @file test_pwm.cpp
 *
 * TIMER::PWM on TIM3 channel 1, PB4 (CN7 pin 19, D25)
 *
 * Copyright (c) 2026 IACE
 */
#include <cmath>
#include <sys/pwm.h>

#include "hwtest.h"

static TIMER::PWM &pwm() {
    // 108MHz timer clock / 108 -> 1MHz, 1000 ticks -> 1kHz
    static TIMER::HW tim(TIM3, 107, 1000);
    static TIMER::PWM pwm({
        .tim = &tim,
        .chan = TIM_CHANNEL_1,
        .pin = AFIO(GPIO_PIN_4, GPIOB, GPIO_AF2_TIM3),
    });
    return pwm;
}

void test_pwm_registers() {
    auto &p = pwm();
    CHECK_EQ(p.period(), 1000);
    CHECK_EQ(TIM3->ARR, 1000);

    p.pwm(0.25);
    CHECK_EQ(TIM3->CCR1, 250);
    CHECK_EQ(p.ticks(), 250);

    p.ticks(600);
    CHECK_EQ(TIM3->CCR1, 600);
    CHECK(std::fabs(p.pwm() - 0.6) < 1e-9);

    // the duty cycle is kept when changing the period
    p.period(2000);
    CHECK_EQ(TIM3->ARR, 2000);
    CHECK_EQ(TIM3->CCR1, 1200);

    // blocked counter must not produce inf/nan duty cycles
    p.period(0);
    p.ticks(10);
    CHECK(std::isfinite(p.pwm()));
    CHECK(p.pwm() == 0);

    p.period(1000);
    CHECK_EQ(TIM3->ARR, 1000);

    // out of range duty cycles are clamped
    p.pwm(1.5);
    CHECK_EQ(TIM3->CCR1, 1000);
    CHECK(p.pwm() == 1);
    p.pwm(-0.5);
    CHECK_EQ(TIM3->CCR1, 0);
    p.pwm(NAN);
    CHECK_EQ(TIM3->CCR1, 0);
    CHECK(p.pwm() == 0);

    p.pwm(0);
    CHECK_EQ(TIM3->CCR1, 0);
}

/** fraction of time the pin is high, sampled at pseudo random times */
static double highfraction(uint32_t ms) {
    uint32_t seed = 0x1234, high = 0, n = 0, start = uwTick;
    while (uwTick - start < ms) {
        seed = seed * 1664525 + 1013904223;
        board::busy_us(1 + (seed >> 24) % 37);
        high += (GPIOB->IDR & GPIO_PIN_4) != 0;
        ++n;
    }
    return (double)high / n;
}

void test_pwm_output() {
    auto &p = pwm();
    for (double duty : {0.1, 0.5, 0.9}) {
        p.pwm(duty);
        double measured = highfraction(100);
        CHECK(std::fabs(measured - duty) < 0.05);
        hwt::info("duty %.2f measured %.3f", duty, measured);
    }
    p.pwm(0);
    // a new compare value takes effect with the next update event
    hwt::sleep(2);
    CHECK(highfraction(10) == 0);
}
