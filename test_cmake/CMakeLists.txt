cmake_minimum_required(VERSION 3.30.0)
project(main VERSION 0.2.5 LANGUAGES C)

find_package(solidc REQUIRED)

add_executable(main  ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
target_link_libraries(main PRIVATE solidc::solidc)


