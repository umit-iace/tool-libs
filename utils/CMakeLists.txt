project(ToolLibsTests)
cmake_minimum_required(VERSION 3.7)

#set(BOOST_INCLUDEDIR /usr/include)
find_package(Boost COMPONENTS unit_test_framework)
if (NOT Boost_FOUND)
    error("Need BOOST for tests")
endif()

enable_testing()

add_executable(QueueTest RequestQueueTest.cpp)
target_compile_definitions(QueueTest PUBLIC BOOST_TEST_DYN_LINK)
target_include_directories(QueueTest PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(QueueTest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test(NAME QueueTest COMMAND QueueTest)

add_executable(MATest MovingAverageTest.cpp)
target_compile_definitions(MATest PUBLIC BOOST_TEST_DYN_LINK)
target_include_directories(MATest PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(MATest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test(NAME MovingAverageTest COMMAND MATest)

add_executable(IntTest InterpolationTest.cpp)
target_compile_definitions(IntTest PUBLIC BOOST_TEST_DYN_LINK)
target_include_directories(IntTest PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(IntTest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test(NAME InterpolationTest COMMAND IntTest)

add_executable(DATest DynamicArrayTest.cpp)
target_compile_definitions(DATest PUBLIC BOOST_TEST_DYN_LINK)
target_include_directories(DATest PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(DATest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test(NAME DynamicArrayTest COMMAND DATest)
