
add_library(tool-libs-stm INTERFACE)
target_sources(tool-libs-stm INTERFACE hal.cpp)
target_link_libraries(tool-libs-stm INTERFACE tool-libs)
target_compile_definitions(tool-libs-stm INTERFACE
    HAL_MODULE_ENABLED HAL_CORTEX_MODULE_ENABLED HAL_GPIO_MODULE_ENABLED
    HAL_FLASH_MODULE_ENABLED HAL_PWR_MODULE_ENABLED
    HAL_DMA_MODULE_ENABLED HAL_RCC_MODULE_ENABLED
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
