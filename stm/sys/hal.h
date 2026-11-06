/** @file hal.h
 *
 * Copyright (c) 2020 IACE
 */
#pragma once

#ifdef STM32F4
	#include "stm32f4xx_hal.h"
#endif

#ifdef STM32F7
	#include "stm32f7xx_hal.h"
#endif

/** HAL_Init, exactly once
 *
 * the peripheral drivers call this in their constructors: global driver
 * objects may be constructed before the global initialization in hal.cpp
 */
void halInit();
