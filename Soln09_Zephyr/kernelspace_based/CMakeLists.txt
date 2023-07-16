cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(Soln09_Zephyr)

target_include_directories(app PRIVATE inc/)

FILE(GLOB c_sources src/*.c)
FILE(GLOB cpp_sources src/*.cpp)

target_sources(app PRIVATE ${c_sources} ${cpp_sources})
