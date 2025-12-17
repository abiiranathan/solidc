cmake_minimum_required(VERSION 3.16)
project(example LANGUAGES C)

find_package(solidc REQUIRED)
add_executable(example main.c)
target_link_libraries(example solidc)
