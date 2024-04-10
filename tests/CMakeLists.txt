include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare( doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG v2.4.10
    GIT_PROGRESS TRUE
    FIND_PACKAGE_ARGS # fallback to git only if not found locally
)
FetchContent_MakeAvailable(doctest)

function(make_test name)
	add_executable(test-${name} main.cpp ${name}.cpp)
	target_compile_features(test-${name} PRIVATE cxx_std_17)
	target_include_directories(test-${name} PUBLIC ${DOCTEST_INCLUDE_DIR})
	target_link_libraries(test-${name} PUBLIC tool-libs doctest::doctest ${ARGN})

	add_custom_target(test-${name}-run
		DEPENDS test-${name}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMAND ./test-${name}
		VERBATIM
	)
	set(TOOL-LIBS-TESTS "test-${name}-run;${TOOL-LIBS-TESTS}" PARENT_SCOPE)
endfunction()

make_test(bitstream)
make_test(buffer)
make_test(experiment)
make_test(frameregistry)
# make_test(interpolation)
make_test(later)
make_test(min)
make_test(movingaverage)
make_test(Queue)
make_test(TFR)

add_custom_target(test-run
	DEPENDS ${TOOL-LIBS-TESTS}
)
