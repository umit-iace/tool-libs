include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare( doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG v2.4.10
    GIT_PROGRESS TRUE
    FIND_PACKAGE_ARGS # fallback to git only if not found locally
)
FetchContent_MakeAvailable(doctest)

function(make_test_standalone name)
	add_executable(test-${name} ${name}.cpp)
	target_compile_features(test-${name} PRIVATE cxx_std_20)
	target_include_directories(test-${name} PUBLIC ${DOCTEST_INCLUDE_DIR})
	target_link_libraries(test-${name} PUBLIC tool-libs doctest::doctest ${ARGN})

	add_custom_target(test-${name}-run
		DEPENDS test-${name}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMAND ./test-${name}
		VERBATIM
	)
endfunction()

function(make_test name)
	make_test_standalone(${name} ${ARGN})
	target_sources(test-${name} PRIVATE main.cpp)
	set(TOOL-LIBS-TESTS "test-${name}-run;${TOOL-LIBS-TESTS}" PARENT_SCOPE)
endfunction()

make_test(bitstream)
make_test(buffer)
make_test(experiment)
make_test(frameregistry)
make_test(interpolation)
make_test(later)
make_test(min)
make_test(movingaverage)
make_test(Queue)
make_test(ndebug)
make_test(kernel)
# Kernel k is defined after the globals of kernel.cpp
target_sources(test-kernel PRIVATE kernel_k.cpp)
make_test(TFR)
make_test(logger)
make_test(slice)
make_test(schedule)
make_test(heartbeat)
make_test(status)
make_test(telemetry)
make_test(canopen)
make_test(bufferutils)
make_test(sim tool-libs-linux)
make_test(bno085)
target_include_directories(test-bno085 PRIVATE stub)
make_test(series)
make_test(linuxcomm tool-libs-linux)
make_test(canframe tool-libs-linux-can)
make_test(linuxhal tool-libs-linux)
make_test(odrive)
make_test(i2cdevices)
target_include_directories(test-i2cdevices PRIVATE stub)
make_test(spidevices)
target_include_directories(test-spidevices PRIVATE stub)
# register bits are or-ed together, keep that free of enum arithmetic
target_compile_options(test-i2cdevices PRIVATE -Werror=deprecated-enum-enum-conversion)
target_compile_options(test-spidevices PRIVATE -Werror=deprecated-enum-enum-conversion)

make_test_standalone(canlinux tool-libs-linux-can)
make_test_standalone(iface tool-libs-linux)

add_custom_target(test-run
	DEPENDS ${TOOL-LIBS-TESTS}
)
