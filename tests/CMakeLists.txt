project(libs_tests VERSION 0.0.1 LANGUAGES CXX)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare( doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG v2.4.10
    GIT_PROGRESS TRUE
    FIND_PACKAGE_ARGS # fallback to git only if not found locally
)
FetchContent_MakeAvailable(doctest)

# Make test executable
add_executable(tool-libs-tests main.cpp
    buffer.cpp
    experiment.cpp
    frameregistry.cpp
    interpolation.cpp
    later.cpp
    min.cpp
    movingaverage.cpp
    Queue.cpp
    requestqueue.cpp
    TFR.cpp
    )
target_compile_features(tool-libs-tests PRIVATE cxx_std_17)
target_include_directories(tool-libs-tests PUBLIC ${DOCTEST_INCLUDE_DIR})
target_link_libraries(tool-libs-tests PUBLIC tool-libs doctest::doctest)

add_custom_target(tool-libs-tests-run
    DEPENDS tool-libs-tests
    COMMAND valgrind -s --leak-check=full "${CMAKE_CURRENT_BINARY_DIR}/tool-libs-tests"
    )
