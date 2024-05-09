#include <core/kern.h>

#include "sys/hal.h"

HAL_StatusTypeDef hal = HAL_Init();
void Kernel::idle() {
    asm("wfi");
}
void Kernel::setTimeStep(uint16_t dt_us) {
    assert(false); // impossible to change physical time
}
void HAL_IncTick() {
    k.tick(uwTickFreq);
    uwTick += uwTickFreq;
}
void __assert_func(const char *file, int line, const char *func, const char *assert) {
    asm("bkpt");
    while (true);
}

extern "C" {
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void) { }

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void) { while (1) ; }

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void) { while (1) ; }

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void) { while (1) ; }

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void) { while (1) ; }

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
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}
}
