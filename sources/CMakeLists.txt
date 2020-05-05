project(ToolLibs)

add_library(min INTERFACE)
target_sources(min INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Min.cpp)
target_include_directories(min INTERFACE ../)
