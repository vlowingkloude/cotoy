cmake_minimum_required(VERSION 4.2)

project(cotoy)

set(CMAKE_C_STANDARD 11)

add_executable(main main.c)

add_library(cotoy cotoy.c)
target_include_directories(cotoy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(main PRIVATE cotoy)