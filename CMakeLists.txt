cmake_minimum_required(VERSION 3.12)
project(SolidC LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(SOLIDC_MAJOR 1 CACHE STRING "major version" FORCE)
set(SOLIDC_MINOR 0 CACHE STRING "minor version" FORCE)
set(SOLIDC ${SOLIDC_MAJOR}.${SOLIDC_MINOR} CACHE STRING "version" FORCE)

# Add the library sources
set(SOLIDC_SOURCES
    input.c
    os.c
    bplustree.c
    linkedlist.c
    map.c
    priority_queue.c
    sort.c
    str.c
    arena.c
    net.c
    threadpool.c
    pubsub.c
    flag.c
)

# Define the library target
add_library(solidc STATIC ${SOLIDC_SOURCES})

# Link to headers and Specify the include directories for the library
target_include_directories(solidc
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Generate the export file
install(TARGETS solidc
  EXPORT solidc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Export the project
install(EXPORT solidc
  FILE solidc.cmake
  NAMESPACE solidc::
  DESTINATION lib/cmake/solidc
)

# Install the header files to the installation directory
# Destinations are relative to CMAKE_INSTALL_PREFIX.
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION include/solidc)

# Install config file
install(FILES solidc-config.cmake DESTINATION lib/cmake/solidc)