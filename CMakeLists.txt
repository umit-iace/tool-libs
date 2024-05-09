cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)

project(tool-libs
	HOMEPAGE_URL https://github.com/umit-iace/tool-libs
	LANGUAGES C CXX ASM
)

option(TEST "generate test targets" FALSE)

add_library(tool-libs INTERFACE)
target_include_directories(tool-libs INTERFACE .)

if(STM32_TOOLCHAIN_PATH)
    add_subdirectory(stm)
else()
    add_subdirectory(linux)
endif()

if (PROJECT_IS_TOP_LEVEL OR TEST)
    add_subdirectory(tests)
endif()

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
