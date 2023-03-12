project(tool-libs)
cmake_minimum_required(VERSION 3.7)

add_library(tool-libs INTERFACE)
target_include_directories(tool-libs INTERFACE .)

add_subdirectory(stm)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(tool-libs-docs
        COMMAND ${CMAKE_COMMAND} -E env TOOL_LIBS_DOCS_OUTPUT=${CMAKE_CURRENT_BINARY_DIR} ${DOXYGEN_EXECUTABLE} docs/doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generate tool-libs documentation"
        VERBATIM
    )
    add_custom_command(TARGET tool-libs-docs POST_BUILD
        COMMENT "tool-libs documentation entry point\nxdg-open ${CMAKE_CURRENT_BINARY_DIR}/html/index.html"
    )
endif()
