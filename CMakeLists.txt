cmake_minimum_required(VERSION 3.16)
project(getch C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_COLOR_MAKEFILE ON)

option(GETCH_ENABLE_ALIASES "Also build getch() and ungetch()" OFF)

add_library(getch SHARED getch.c getch.h)
add_library(getch_static STATIC getch.c getch.h)

if (GETCH_ENABLE_ALIASES)
    target_compile_definitions(getch PUBLIC GETCH_ENABLE_ALIASES)
    target_compile_definitions(getch_static PUBLIC GETCH_ENABLE_ALIASES)
endif ()

add_executable(example example.c)
add_executable(getchTest tests/test.c tests/getchTests.c)

find_library(UTIL_LIBRARY util)
target_link_libraries(getchTest getch ${UTIL_LIBRARY})
target_link_libraries(example getch)

if (ENABLE_TESTS EQUAL 1)
    enable_testing()

    add_test(NAME defaultTests
            COMMAND getchTest
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )

    foreach (suite cinPeekTests getchPtyTests eventDeviceTests)
        add_test(NAME ${suite}
                COMMAND getchTest ${suite}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                )
    endforeach ()
endif ()

if (ENABLE_TESTS EQUAL 1)
    add_executable(linkSharedSmoke tests/linkSmoke.c)
    target_link_libraries(linkSharedSmoke getch)
    add_executable(linkStaticSmoke tests/linkSmoke.c)
    target_link_libraries(linkStaticSmoke getch_static)

    add_test(NAME linkSharedSmoke COMMAND linkSharedSmoke)
    add_test(NAME linkStaticSmoke COMMAND linkStaticSmoke)

    # The project is C only, so build the C++ check by hand when a C++ compiler exists.
    find_program(GETCH_CXX NAMES c++ g++ clang++)
    if (GETCH_CXX)
        add_test(NAME headerWorksFromCxx
                COMMAND sh -c "\"${GETCH_CXX}\" \"${CMAKE_SOURCE_DIR}/tests/cppSmoke.cpp\" \"$<TARGET_FILE:getch_static>\" -o cppSmoke && ./cppSmoke"
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                )
    endif ()

    # getch()/ungetch() must be exported only when GETCH_ENABLE_ALIASES is on.
    find_program(GETCH_NM nm)
    if (GETCH_NM)
        set(aliasCheck "\"${GETCH_NM}\" -D --defined-only \"$<TARGET_FILE:getch>\" | grep -qw getch")
        if (NOT GETCH_ENABLE_ALIASES)
            set(aliasCheck "! ${aliasCheck}")
        endif ()
        add_test(NAME aliasSymbols COMMAND sh -c "${aliasCheck}")
    endif ()
endif ()

add_custom_target(escapeTest getchTest escapeReturnsTest
        COMMAND getchTest specialKeyTest
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        USES_TERMINAL
        )

set_target_properties(getch PROPERTIES PUBLIC_HEADER getch.h)

include(GNUInstallDirs)

install(TARGETS getch getch_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
