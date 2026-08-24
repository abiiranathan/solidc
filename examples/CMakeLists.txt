# Examples
#
# Two build modes:
#
#   1. In-tree (preferred for development): from the repository root with
#          cmake -DBUILD_EXAMPLES=ON ..
#      Links against the solidc target built in the same tree, so example
#      changes are tested against library changes, not a system install.
#
#   2. Standalone: from this directory against an installed package
#          cmake -S . -B build && cmake --build build
#      Locates solidc via find_package (solidcConfig.cmake).

if(NOT TARGET solidc)
    cmake_minimum_required(VERSION 3.16)
    project(example LANGUAGES C)

    find_package(solidc CONFIG REQUIRED)
    set(SOLIDC_LINK_LIB solidc::solidc)
else()
    # In-tree build from the root project.
    set(SOLIDC_LINK_LIB solidc)
endif()

add_executable(example main.c)
target_link_libraries(example PRIVATE ${SOLIDC_LINK_LIB})

add_executable(graphics graphics.c)
target_link_libraries(graphics PRIVATE ${SOLIDC_LINK_LIB})

add_executable(ml_prototype ml_prototype.c)
target_link_libraries(ml_prototype PRIVATE ${SOLIDC_LINK_LIB})
