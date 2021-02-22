cmake_minimum_required(VERSION 3.16)
project(getch C)

set(CMAKE_C_STANDARD 11)

add_library(getch SHARED getch.c getch.h)
add_executable(test test.c )

target_link_libraries(test -L./build getch)