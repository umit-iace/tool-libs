#include <core/kern.h>
#include <core/critical.h>

#include "sys/hal.h"

void halInit() {
    static const HAL_StatusTypeDef status = HAL_Init();
    (void)status;
}
// also before main when there are no global drivers
[[maybe_unused]] static const bool hal = (halInit(), true);
void Kernel::idle() {
    asm("wfi");
}
void Kernel::setTimeStep(uint16_t dt_us) {
    assert(false); // impossible to change physical time
}
void __assert_func(const char *file, int line, const char *func, const char *assert) {
    asm("bkpt");
    while (true);
}

extern "C" {
/* make newlib's malloc & free interrupt safe:
 * buffers get (de)allocated from interrupt handlers as well as from the
 * main loop. newlib calls these around every heap operation, possibly nested.
 */
static uint32_t malloc_primask;
static uint32_t malloc_nesting;
void __malloc_lock(struct _reent *) {
    uint32_t primask = CriticalSection::enter();
    if (malloc_nesting++ == 0) malloc_primask = primask;
}
void __malloc_unlock(struct _reent *) {
    if (--malloc_nesting == 0) CriticalSection::leave(malloc_primask);
}

/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void) { }

/* The fault handlers below just halt. They are weak: an application driving
 * hardware that must not keep running on a fault (e.g. motor PWM) defines
 * its own to switch it off first. Also reached by a failed assert without a
 * debugger (bkpt escalates to HardFault).
 */

/**
  * @brief This function handles Hard fault interrupt.
  */
__attribute__((weak)) void HardFault_Handler(void) { while (1) ; }

/**
  * @brief This function handles Memory management fault.
  */
__attribute__((weak)) void MemManage_Handler(void) { while (1) ; }

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
__attribute__((weak)) void BusFault_Handler(void) { while (1) ; }

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
__attribute__((weak)) void UsageFault_Handler(void) { while (1) ; }

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void) { }

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void) { }

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void) { }

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void) {
    k.tick(uwTickFreq);
    uwTick += uwTickFreq;
}
}
