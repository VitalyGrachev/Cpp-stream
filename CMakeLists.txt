cmake_minimum_required(VERSION 3.8)
project(cpp_stream)

set(CMAKE_CXX_STANDARD 17)

add_library(cpp_stream INTERFACE)

target_include_directories(cpp_stream INTERFACE include/)

if(NOT TARGET gtest)
    if ((NOT gtest_SOURCE_DIR) OR (NOT EXISTS ${gtest_SOURCE_DIR}))
        execute_process(COMMAND git submodule update --init -- lib/googletest
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()

    add_subdirectory(lib/googletest/googletest)
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
endif()

add_executable(cpp_stream_tests tests/stream_tests.cpp)

target_link_libraries(cpp_stream_tests cpp_stream gtest gtest_main)
