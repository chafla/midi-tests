# cmake_minimum_required(VERSION <specify CMake version here>)
project(midis)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c track.c track.h)
add_executable(midis ${SOURCE_FILES})