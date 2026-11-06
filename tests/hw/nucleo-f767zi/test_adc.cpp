/** @file test_adc.cpp
 *
 * ADC::HW on ADC1 with DMA2 stream 0, measuring the internal reference
 * voltage, no wiring needed. The D-cache is on, so the samples the DMA
 * writes are only seen if the driver invalidates the cache lines.
 *
 * Copyright (c) 2026 IACE
 */
#include <sys/adc.h>

#include "hwtest.h"

void test_adc_dcache() {
    __HAL_RCC_DMA2_CLK_ENABLE();
    // in cached SRAM1, the DTCM below it isn't cached
    __attribute__((section(".sram1")))
    static ADC::HW adc({
        .adc = ADC1,
        .prescaler = ADC_CLOCK_SYNC_PCLK_DIV4,
        .dmaStream = DMA2_Stream0,
        .dmaChannel = DMA_CHANNEL_0,
    });
    adc.init();
    // VREFINT has no pin, ChannelConf wants one: A0 as analog input
    auto vref = adc.get({
        .channel = ADC_CHANNEL_VREFINT,
        .samplingTime = ADC_SAMPLETIME_480CYCLES,
        .lsb = 3.3 / 4095,
        .pin = DIO(GPIO_PIN_3, GPIOA, GPIO_MODE_ANALOG),
    });
    ADC123_COMMON->CCR |= ADC_CCR_TSVREFE;
    hwt::sleep(1); // VREFINT start up 10us

    CHECK((uint32_t)vref.samples >= 0x20020000);
    // a dirty cache line over the samples, as left by the constructor
    vref.samples->raw[vref.ix] = 0x777;
    hwt::barrier();
    adc.measure();
    hwt::sleep(1); // one conversion: 480 + 12 cycles at 27MHz

    // factory calibration: raw VREFINT at 3.3V, 30°C
    uint16_t cal = *(const uint16_t *)0x1FF0F44A;
    double volts = vref.get(); // with a stale cache line: still 0x777
    double raw = volts / vref.lsb;
    CHECK(raw > cal * 0.9 && raw < cal * 1.1);
    CHECK(volts > 1.1 && volts < 1.35);
    hwt::info("VREFINT raw %.0f (calibration %u), %.3f V", raw, cal, volts);
}
