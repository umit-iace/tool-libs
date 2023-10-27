add_library(tool-libs-linux INTERFACE)
target_sources(tool-libs-linux INTERFACE host.cpp)
target_link_libraries(tool-libs-linux INTERFACE tool-libs)
