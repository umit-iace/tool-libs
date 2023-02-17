set(SOURCES
    hal.cpp
    uart.cpp
)

cmake_policy(VERSION 3.24)

add_library(tool-libs-stm INTERFACE)
target_sources(tool-libs-stm INTERFACE
    ${SOURCES}
)
