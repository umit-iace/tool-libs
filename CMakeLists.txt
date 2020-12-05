project(tool-libs)
cmake_minimum_required(VERSION 3.7)

add_library(tool-libs INTERFACE)
target_include_directories(tool-libs INTERFACE .)
