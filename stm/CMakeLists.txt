include(FetchContent)
FetchContent_Declare(stm_cmake
    GIT_REPOSITORY https://github.com/ObKo/stm32-cmake
    GIT_TAG v2.1.0
)
FetchContent_MakeAvailable(stm_cmake)

macro(tool_libs_stm_enable)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED PROJECT_NAME)
        message(FATAL_ERROR "tool_libs_stm_enable() MUST be called before calling project()")
    else()
        message("tlch: ${CMAKE_TOOLCHAIN_FILE} ; prj: ${PROJECT_NAME}")
        message("passed ^^")
    endif()
    set(CMAKE_TOOLCHAIN_FILE ${stm_cmake_SOURCE_DIR}cmake/stm32_gcc.cmake)
    message("toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    # return(PROPAGATE CMAKE_TOOLCHAIN_FILE)
endmacro()


function(tool_libs_stm_link TARGET CHIP)
    set(FAM "")
    set(DEV "")
    stm32_get_chip_info(${CHIP} FAM DEV)
    target_link_libraries(${TARGET} INTERFACE
        CMSIS::STM32::${DEV}
        HAL::STM32::${FAM}::CORTEX
    )
    foreach(LNK IN LISTS ${ARGN})
        target_link_libraries(${TARGET} INTERFACE
            HAL::STM32::${FAM}::${LNK}
        )
    endforeach()
endfunction()


add_library(tool-libs-stm INTERFACE)
target_sources(tool-libs-stm INTERFACE hal.cpp)
target_link_libraries(tool-libs-stm INTERFACE tool-libs)
target_compile_definitions(tool-libs-stm INTERFACE
    HAL_TIM_MODULE_ENABLED USE_HAL_TIM_REGISTER_CALLBACKS=1
    )
target_include_directories(tool-libs-stm INTERFACE .)

add_library(tool-libs-stm-i2c INTERFACE)
target_sources(tool-libs-stm-i2c INTERFACE i2c.cpp)
target_link_libraries(tool-libs-stm-i2c INTERFACE tool-libs-stm)
target_compile_definitions(tool-libs-stm-i2c INTERFACE
    HAL_I2C_MODULE_ENABLED USE_HAL_I2C_REGISTER_CALLBACKS=1)

add_library(tool-libs-stm-spi INTERFACE)
target_sources(tool-libs-stm-spi INTERFACE spi.cpp)
target_link_libraries(tool-libs-stm-spi INTERFACE tool-libs-stm)
target_compile_definitions(tool-libs-stm-spi INTERFACE
    HAL_SPI_MODULE_ENABLED USE_HAL_SPI_REGISTER_CALLBACKS=1)

add_library(tool-libs-stm-uart INTERFACE)
target_sources(tool-libs-stm-uart INTERFACE uart.cpp)
target_link_libraries(tool-libs-stm-uart INTERFACE tool-libs-stm)
target_compile_definitions(tool-libs-stm-uart INTERFACE
    HAL_UART_MODULE_ENABLED USE_HAL_UART_REGISTER_CALLBACKS=1)

add_library(tool-libs-stm-can INTERFACE)
target_sources(tool-libs-stm-can INTERFACE can.cpp)
target_link_libraries(tool-libs-stm-can INTERFACE tool-libs-stm)
target_compile_definitions(tool-libs-stm-can INTERFACE
    HAL_CAN_MODULE_ENABLED USE_HAL_CAN_REGISTER_CALLBACKS=1)
