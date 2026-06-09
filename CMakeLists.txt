cmake_minimum_required(VERSION 3.28)

project(
    mparse
    VERSION 0.1.0
    DESCRIPTION "grammar parser"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MPARSE_BUILD_TESTS "Build mparse tests" ON)

include(Dependencies.cmake)

add_subdirectory(include)
add_subdirectory(src)

if(MPARSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
