cmake_minimum_required(VERSION 3.30.0)
project(solidc VERSION 0.2.5 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR WIN32 OR MINGW OR CMAKE_C_COMPILER MATCHES "mingw")
    set(IS_WINDOWS_TARGET TRUE)
    message(STATUS "Building for Windows target")
else()
    set(IS_WINDOWS_TARGET FALSE)
    message(STATUS "Building for non-Windows target")
endif()


# Fix RPATH issue with Ninja + Windows
if(UNIX AND NOT APPLE)
    # Set RPATH on Linux
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(BUILD_SHARED_LIBS AND (WIN32 OR MINGW))
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
option(BUILD_TESTS "Build the tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Workaround for GNUInstallDirs warning during cross-compilation
if(CMAKE_CROSSCOMPILING)
    set(CMAKE_INSTALL_LIBDIR "lib")
    set(CMAKE_INSTALL_BINDIR "bin")
    set(CMAKE_INSTALL_INCLUDEDIR "include")
else()
    include(GNUInstallDirs)
endif()

include(CTest)
enable_testing()

# Header files
set(HEADERS 
    include/arena.h
    include/cstr.h
    include/csvparser.h
    include/file.h
    include/filepath.h
    include/flag.h
    include/hash.h
    include/list.h
    include/lock.h
    include/map.h
    include/pipeline.h
    include/process.h
    include/slist.h
    include/socket.h
    include/stdstreams.h
    include/strton.h
    include/thread.h
    include/threadpool.h
    include/unicode.h
    include/defer.h

    # Combined header
    include/solidc.h
)

# Source files
set(SOURCES
    src/arena.c
    src/cstr.c
    src/csvparser.c
    src/file.c
    src/filepath.c
    src/flag.c
    src/hash.c
    src/list.c
    src/lock.c
    src/map.c
    src/pipeline.c
    src/process.c
    src/slist.c
    src/socket.c
    src/stdstreams.c
    src/strton.c
    src/thread.c
    src/threadpool.c
    src/unicode.c
)

# Create the library
add_library(solidc STATIC ${SOURCES})
add_library(solidc::solidc ALIAS solidc)

# Headers
target_include_directories(solidc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/>
)

# Platform-specific settings
if(WIN32 OR MINGW)    
    # Windows-specific compiler flags
    target_compile_definitions(solidc PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    # Windows-specific linker flags
    target_link_libraries(solidc PUBLIC ws2_32 advapi32 kernel32)

    # Disable warnings that are too noisy on MSVC
    if(MSVC)
        target_compile_options(solidc PRIVATE
            /W4
            /wd4201  # nameless struct/union
            /wd4996  # POSIX name deprecated
        )
    endif()
else()
    # Unix-like compiler flags
    target_link_libraries(solidc PUBLIC m pthread)
    
    # Common warnings for GCC/Clang
    target_compile_options(solidc PRIVATE
        -Wall
        -Wextra
        -pedantic
        -Werror
        -std=c23
    )
    
    # Security hardening in Release mode
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(solidc PRIVATE
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
            -fPIE
            -fPIC
        )
        target_link_options(solidc PRIVATE
            -pie
        )
    endif()
    
    # Clang-specific settings
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(solidc PRIVATE 
            -Wno-gnu-zero-variadic-macro-arguments
            -fblocks
        )
        target_link_libraries(solidc PRIVATE BlocksRuntime)
    endif()
endif()

# Installation: headers and library
install(TARGETS solidc
    EXPORT solidc_export
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/solidc
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/solidc)

# Export config
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

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/solidcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/solidcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc
)

install(EXPORT solidc_export
    FILE solidcTargets.cmake
    NAMESPACE solidc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/solidc
)

# Pkg-config - only on Unix-like systems
if(NOT WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/solidc.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/solidc.pc @ONLY)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/solidc.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# Tests and benchmarks
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(NOT WIN32)
    add_subdirectory(benchmarks)
endif()