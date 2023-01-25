set(SOURCES
    hal.cpp
    uart.cpp
)

cmake_policy(VERSION 3.24)
target_sources(tool-libs INTERFACE
    ${SOURCES}
)
