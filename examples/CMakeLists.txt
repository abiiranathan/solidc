cmake_minimum_required(VERSION 3.16)
project(example LANGUAGES C)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-march=native)
endif()

find_package(solidc REQUIRED)
add_executable(example main.c)
target_link_libraries(example solidc m)

add_executable(graphics graphics.c)
target_link_libraries(graphics solidc m)


