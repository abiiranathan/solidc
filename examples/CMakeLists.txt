cmake_minimum_required(VERSION 3.16)
project(example LANGUAGES C)

find_package(solidc CONFIG REQUIRED)
add_executable(example main.c)
target_link_libraries(example solidc::solidc)

add_executable(graphics graphics.c)
target_link_libraries(graphics solidc::solidc)