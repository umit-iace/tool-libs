add_library(tool-libs-linux INTERFACE)
target_sources(tool-libs-linux INTERFACE hal.cpp)
target_link_libraries(tool-libs-linux INTERFACE tool-libs)
target_include_directories(tool-libs-linux INTERFACE .)

add_library(tool-libs-linux-can INTERFACE)
# target_sources(tool-libs-linux-can INTERFACE can.cpp)
target_link_libraries(tool-libs-linux-can INTERFACE tool-libs-linux)
