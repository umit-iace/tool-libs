add_library(tool-libs-stm INTERFACE)
target_sources(tool-libs-stm INTERFACE
    hal.cpp
    uart.cpp
)
