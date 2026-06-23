cmake_minimum_required(VERSION 3.12)
project(HashTable)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror
    -Wold-style-cast -Wcast-align -Wunused -Wnull-dereference -Wdouble-promotion)

add_executable(tests tests.cpp)
target_compile_options(tests PRIVATE -O0 -g ${COMMON_WARNINGS})

add_executable(bench bench.cpp)
target_compile_options(bench PRIVATE -O3 ${COMMON_WARNINGS})
