/** @file hal.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef STM_HAL_H
#define STM_HAL_H

#ifdef STM32F4
	#include "stm32f4xx_hal.h"
#endif

#ifdef STM32F7
	#include "stm32f7xx_hal.h"
#endif

#ifdef STM32H7
	#include "stm32h7xx_hal.h"
#endif

#endif //STM_HAL_H
