set(SOURCES
    hal.c
    uart.cpp
)
target_sources(tool-libs INTERFACE
    $<PATH:APPEND,${CMAKE_CURRENT_SOURCE_DIR},${SOURCES}>
)
