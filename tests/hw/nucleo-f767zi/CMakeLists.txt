# tool-libs hardware tests for the NUCLEO-F767ZI
#
# standalone project, see README.md. usually driven by run.py
cmake_minimum_required(VERSION 3.25)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/arm-none-eabi.cmake CACHE FILEPATH "")

project(hwtest-nucleo-f767zi LANGUAGES C CXX ASM)

set(HWTEST_BAUD 115200 CACHE STRING "baudrate of the ST-Link virtual COM port")
option(HWTEST_PRINTF_FLOAT "link printf with float support (-u _printf_float)" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON) # tool-libs uses M_PI
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ST sources, pinned. use -DFETCHCONTENT_SOURCE_DIR_<NAME>=... for local copies
include(FetchContent)
FetchContent_Declare(stm32f7_hal
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32f7xx_hal_driver.git
    GIT_TAG v1.3.3
    GIT_SHALLOW TRUE)
FetchContent_Declare(cmsis_device_f7
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_f7.git
    GIT_TAG v1.2.10
    GIT_SHALLOW TRUE)
FetchContent_Declare(cmsis_core
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_core.git
    GIT_TAG v5.9.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(stm32f7_hal cmsis_device_f7 cmsis_core)

set(MCU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
add_compile_options(${MCU_FLAGS}
    -O2 -g3 -ffunction-sections -fdata-sections -Wall
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fno-use-cxa-atexit>"
)
add_compile_definitions(STM32F767xx STM32F7 USE_HAL_DRIVER)
add_link_options(${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F767ZITx_FLASH.ld
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/hwtest.map
    $<$<BOOL:${HWTEST_PRINTF_FLOAT}>:-u_printf_float>
    # counts why stm/uart.cpp aborts transfers, see main.cpp
    -Wl,--wrap=HAL_UART_AbortReceive -Wl,--wrap=HAL_UART_AbortTransmit
)

# HAL & CMSIS, compiled as part of the firmware so they see the same
# compile definitions (register callbacks!) as tool-libs
add_library(stm32f7-hal INTERFACE)
target_include_directories(stm32f7-hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${stm32f7_hal_SOURCE_DIR}/Inc
    ${cmsis_device_f7_SOURCE_DIR}/Include
    ${cmsis_core_SOURCE_DIR}/Include
)
foreach(m hal hal_cortex hal_rcc hal_rcc_ex hal_pwr hal_pwr_ex hal_flash hal_flash_ex
        hal_gpio hal_dma hal_dma_ex hal_uart hal_uart_ex hal_tim hal_tim_ex
        hal_spi hal_spi_ex hal_i2c hal_i2c_ex hal_can hal_adc hal_adc_ex)
    target_sources(stm32f7-hal INTERFACE ${stm32f7_hal_SOURCE_DIR}/Src/stm32f7xx_${m}.c)
endforeach()
target_sources(stm32f7-hal INTERFACE
    ${cmsis_device_f7_SOURCE_DIR}/Source/Templates/gcc/startup_stm32f767xx.s
    ${cmsis_device_f7_SOURCE_DIR}/Source/Templates/system_stm32f7xx.c
)
# silence warnings in vendor code
set_source_files_properties(
    ${cmsis_device_f7_SOURCE_DIR}/Source/Templates/system_stm32f7xx.c
    PROPERTIES COMPILE_OPTIONS -w)

# tool-libs itself, the STM32_TOOLCHAIN_PATH switch selects the stm backend
set(STM32_TOOLCHAIN_PATH ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. tool-libs EXCLUDE_FROM_ALL)

add_executable(hwtest
    # before main.cpp (Kernel k): its globals are constructed first
    test_early.cpp
    main.cpp
    board.cpp
    syscalls.c
    test_core.cpp
    test_logger.cpp
    test_pwm.cpp
    test_spi.cpp
    test_i2c.cpp
    test_can.cpp
    test_adc.cpp
)
target_compile_definitions(hwtest PRIVATE HWTEST_BAUD=${HWTEST_BAUD})
target_link_libraries(hwtest PRIVATE
    stm32f7-hal
    tool-libs-stm
    tool-libs-stm-uart
    tool-libs-stm-spi
    tool-libs-stm-i2c
    tool-libs-stm-can
)
set_target_properties(hwtest PROPERTIES SUFFIX .elf)
add_custom_command(TARGET hwtest POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:hwtest> hwtest.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:hwtest>
)

add_custom_target(flash
    COMMAND openocd -f board/st_nucleo_f7.cfg -c "program $<TARGET_FILE:hwtest> verify reset exit"
    DEPENDS hwtest
    USES_TERMINAL)
