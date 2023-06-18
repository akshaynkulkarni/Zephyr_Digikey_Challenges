cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(Soln02_Zephyr)

target_include_directories(app PRIVATE src/inc/)

target_sources(app PRIVATE src/main.cpp src/led.cpp )
