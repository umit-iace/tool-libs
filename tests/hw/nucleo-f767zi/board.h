/** @file board.h
 *
 * NUCLEO-F767ZI board support for the hardware tests
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <sys/hal.h>
#include <cstdint>

namespace UART { struct HW; }
namespace SPI { struct HW; }
namespace I2C { struct HW; }
namespace CAN { struct HW; }

namespace board {
/** priority of the peripheral interrupts under test, SysTick runs at 15 */
constexpr uint32_t IRQ_PRIO = 5;
/** priority of the stress timers TIM6 & TIM7 */
constexpr uint32_t STRESS_PRIO = 6;

/** caches, 216MHz clock tree, GPIO clocks & cycle counter */
void init();
/** CPU cycles since boot (wraps after ~20s) */
inline uint32_t cycles() { return DWT->CYCCNT; }
/** busy wait, independent of interrupts */
void busy_us(uint32_t us);

/** run `hook` from the TIM6 / TIM7 interrupt at `hz` */
void stress6(uint32_t hz, void (*hook)());
void stress7(uint32_t hz, void (*hook)());
/** stop the stress timers */
void stress6_stop();
void stress7_stop();

/** peripherals as created by main / the tests, for the interrupt handlers */
extern UART::HW *vcp;
extern SPI::HW *spi1;
extern I2C::HW *i2c1;
extern CAN::HW *can1;
}
