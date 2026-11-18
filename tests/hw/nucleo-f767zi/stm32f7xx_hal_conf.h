/** @file stm32f7xx_hal_conf.h
 *
 * HAL configuration for the tool-libs hardware tests
 * (reduced from stm32f7xx_hal_conf_template.h, HAL v1.3.3)
 *
 * the tool-libs-stm-* CMake targets enable their modules & register
 * callbacks via compile definitions, hence all the #ifndef guards
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#ifndef HAL_CAN_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#endif
#ifndef HAL_I2C_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#endif
#ifndef HAL_SPI_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#endif
#ifndef HAL_TIM_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#endif
#ifndef HAL_UART_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#endif

/* Nucleo-144: 8MHz MCO from the ST-Link, unused, we run from the HSI */
#define HSE_VALUE                8000000U
#define HSE_STARTUP_TIMEOUT      100U
#define HSI_VALUE                16000000U
#define LSI_VALUE                32000U
#define LSE_VALUE                32768U
#define LSE_STARTUP_TIMEOUT      5000U
#define EXTERNAL_CLOCK_VALUE     12288000U

#define VDD_VALUE                3300U
#define TICK_INT_PRIORITY        0x0FU
#define USE_RTOS                 0U
#define PREFETCH_ENABLE          1U
#define ART_ACCELERATOR_ENABLE   1U

#ifndef USE_HAL_CAN_REGISTER_CALLBACKS
#define USE_HAL_CAN_REGISTER_CALLBACKS   1U
#endif
#ifndef USE_HAL_I2C_REGISTER_CALLBACKS
#define USE_HAL_I2C_REGISTER_CALLBACKS   1U
#endif
#ifndef USE_HAL_SPI_REGISTER_CALLBACKS
#define USE_HAL_SPI_REGISTER_CALLBACKS   1U
#endif
#ifndef USE_HAL_TIM_REGISTER_CALLBACKS
#define USE_HAL_TIM_REGISTER_CALLBACKS   1U
#endif
#ifndef USE_HAL_UART_REGISTER_CALLBACKS
#define USE_HAL_UART_REGISTER_CALLBACKS  1U
#endif

#define USE_SPI_CRC              0U

#include "stm32f7xx_hal_rcc.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_adc.h"
#include "stm32f7xx_hal_cortex.h"
#include "stm32f7xx_hal_can.h"
#include "stm32f7xx_hal_flash.h"
#include "stm32f7xx_hal_i2c.h"
#include "stm32f7xx_hal_pwr.h"
#include "stm32f7xx_hal_spi.h"
#include "stm32f7xx_hal_tim.h"
#include "stm32f7xx_hal_uart.h"

#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif
