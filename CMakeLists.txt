cmake_minimum_required(VERSION 3.30)
project(WFC)

file(GLOB_RECURSE sources src/*.cpp src/*.h)

add_executable(WFC ${sources})

target_include_directories(WFC PUBLIC src/)
