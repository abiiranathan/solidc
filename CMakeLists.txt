cmake_minimum_required(VERSION 3.5.0)
project(solidc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
option(BUILD_TESTS "Build the tests" ON)

include(GNUInstallDirs)
include(CTest)
enable_testing()

# Source files as all .c files in the src directory
file(GLOB SOURCES "src/*.c")

# Header files as all .h files in the include directory
file(GLOB HEADERS "include/*.h")

# Include our main solidc library.
# Can pass -DBUILD_SHARED_LIB to build a shared library
add_library(solidc ${SOURCES})
target_link_libraries(solidc PUBLIC m pthread)

# Include the include directory
target_include_directories(solidc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/solidc>
)

# Configure the target to use the C11 standard
target_compile_options(solidc PRIVATE -Wall -Wextra -pedantic -Werror)

# Explicitly install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/solidc)

# Install the target library
install(TARGETS solidc LIBRARY DESTINATION lib)

# Add the tests
if(BUILD_TESTS)
	add_subdirectory(tests)
endif()


# # ========== Installations ==========
install(TARGETS solidc
        EXPORT solidc_export
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT solidc_export
        FILE solidcTargets.cmake
        NAMESPACE solidc::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/solidcConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc)

# Generate the configuration file for the library
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/solidcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/solidcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/solidcConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/solidcConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc)

# Generate the .pc file for pkg-config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/solidc.pc.in
${CMAKE_CURRENT_BINARY_DIR}/solidc.pc
@ONLY)

# Install the .pc file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/solidc.pc
DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)