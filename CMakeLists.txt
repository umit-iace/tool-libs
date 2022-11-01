project(tool-libs)
cmake_minimum_required(VERSION 3.7)

add_library(tool-libs INTERFACE)
target_sources(tool-libs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stm/uart.cpp)
target_include_directories(tool-libs INTERFACE .)
