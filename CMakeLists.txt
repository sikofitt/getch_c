cmake_minimum_required(VERSION 3.16)
project(getch C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_COLOR_MAKEFILE ON)

add_library(getch SHARED getch.c getch.h)
add_executable(example example.c )
add_executable(getchTest tests/test.c tests/getchTests.c)
add_test(NAME getchTests
        COMMAND getchTest
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(getchTest -L./build getch)
target_link_libraries(example -L./build getch)
if (ENABLE_TESTS EQUAL 1)
    enable_testing()
endif ()

