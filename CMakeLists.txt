cmake_minimum_required(VERSION 3.16)
project(getch C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_COLOR_MAKEFILE ON)

add_library(libgetch SHARED getch.c getch.h)
add_library(libgetch_static STATIC getch.c getch.h)

add_executable(example example.c )
add_executable(getchTest tests/test.c tests/getchTests.c)

add_test(NAME defaultTests
        COMMAND "getchTest"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )

target_link_libraries(getchTest -L./build libgetch)
target_link_libraries(example -L./build libgetch)

if (ENABLE_TESTS EQUAL 1)
    enable_testing()
endif ()

add_custom_target(escapeTest getchTest escapeReturnsTest
        COMMAND getchTest specialKeyTest
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        USES_TERMINAL
        )
set_target_properties(libgetch PROPERTIES PUBLIC_HEADER getch.h)
set_target_properties(libgetch PROPERTIES OUTPUT_NAME getch)
set_target_properties(libgetch_static PROPERTIES OUTPUT_NAME getch_static)

include(GNUInstallDirs)

install(TARGETS libgetch libgetch_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}
        )

