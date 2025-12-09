cmake_minimum_required(VERSION 3.30.0)
project(solidc VERSION 0.2.5 LANGUAGES C)

# ==============================================================================
# Global Settings
# ==============================================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Fix RPATH issue with Ninja + Windows
if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(BUILD_SHARED_LIBS AND (WIN32 OR MINGW))
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTS "Build the tests" ON)
option(BUILD_BENCHMARKS "Build the benchmarks" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF) # Default to Static for easier dependency embedding

# ==============================================================================
# Platform Detection
# ==============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR WIN32 OR MINGW OR CMAKE_C_COMPILER MATCHES "mingw")
    set(IS_WINDOWS_TARGET TRUE)
    message(STATUS "Building for Windows target")
else()
    set(IS_WINDOWS_TARGET FALSE)
    message(STATUS "Building for non-Windows target")
endif()

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(ARCH_X64 TRUE)
    message(STATUS "Target architecture: x86-64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(ARCH_ARM64 TRUE)
    message(STATUS "Target architecture: ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(ARCH_ARM TRUE)
    message(STATUS "Target architecture: ARM")
else()
    message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ==============================================================================
# Dependencies (FetchContent)
# ==============================================================================
include(FetchContent)
message(STATUS "Fetching dependencies...")

# --- xxHash ---
FetchContent_Declare(
    xxHash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG        v0.8.2
)
FetchContent_MakeAvailable(xxHash)

# Create xxHash as an OBJECT library (compiled but not linked as separate lib)
add_library(xxHash_obj OBJECT 
    ${xxhash_SOURCE_DIR}/xxhash.c
)
target_include_directories(xxHash_obj PUBLIC 
    ${xxhash_SOURCE_DIR}
)
# Suppress warnings from xxHash
target_compile_options(xxHash_obj PRIVATE 
    $<$<C_COMPILER_ID:MSVC>:/w>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
)
set_target_properties(xxHash_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- mimalloc ---
set(MI_BUILD_SHARED OFF CACHE BOOL "Build mimalloc shared lib" FORCE)
set(MI_BUILD_STATIC ON CACHE BOOL "Build mimalloc static lib" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)
set(MI_OVERRIDE ON CACHE BOOL "Override standard malloc" FORCE)
set(MI_INSTALL_TOPLEVEL OFF CACHE BOOL "Install mimalloc to top level" FORCE)

FetchContent_Declare(
    mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG        v2.1.7
)
FetchContent_MakeAvailable(mimalloc)

# Ensure mimalloc targets exist as aliases if needed
if(NOT TARGET mimalloc-static AND TARGET mimalloc)
    add_library(mimalloc-static ALIAS mimalloc)
endif()

if(TARGET mimalloc-obj)
    target_compile_options(mimalloc-obj PRIVATE 
        $<$<C_COMPILER_ID:MSVC>:/w>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
    )
endif()
if(TARGET mimalloc-static)
    target_compile_options(mimalloc-static PRIVATE 
        $<$<C_COMPILER_ID:MSVC>:/w>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
    )
endif()

# ==============================================================================
# Project Configuration
# ==============================================================================

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
    include/aligned_alloc.h
    include/arena.h
    include/array.h
    include/cmp.h
    include/cstr.h
    include/csvparser.h
    include/defer.h
    include/file.h
    include/filepath.h
    include/global.h
    include/hash.h
    include/list.h
    include/lock.h
    include/macros.h
    include/map.h
    include/pipeline.h
    include/process.h
    include/slist.h
    include/socket.h
    include/stdstreams.h
    include/str_to_num.h
    include/thread.h
    include/threadpool.h
    include/unicode.h
    include/wintypes.h
    include/linear_alg.h
    include/vec.h
    include/matrix.h
    include/rwlock.h
    include/spinlock.h
    include/cache.h
    include/dynarray.h
    include/dotenv.h
    include/solidc.h
    include/xtime.h
    include/hashset.h
    include/trie.h
    include/flags.h
)

# Source files
set(SOURCES
    src/arena.c
    src/cstr.c
    src/csvparser.c
    src/file.c
    src/filepath.c
    src/hash.c
    src/list.c
    src/lock.c
    src/map.c
    src/pipeline.c
    src/process.c
    src/slist.c
    src/socket.c
    src/stdstreams.c
    src/str_to_num.c
    src/thread.c
    src/threadpool.c
    src/unicode.c
    src/cache.c
    src/dynarray.c
    src/dotenv.c
    src/xtime.c
    src/trie.c
    src/flags.c
)

# Include math.h constants e.g M_PI in C23
add_compile_definitions(_USE_MATH_DEFINES _GNU_SOURCE)

# Create the library - add xxHash object files directly
add_library(solidc STATIC ${SOURCES} $<TARGET_OBJECTS:xxHash_obj>)
add_library(solidc::solidc ALIAS solidc)

# Headers (include xxHash headers for internal use)
target_include_directories(solidc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/>
)
target_include_directories(solidc PRIVATE
    ${xxhash_SOURCE_DIR}
)

# Link Dependencies - only mimalloc now, xxHash is embedded via object files
target_link_libraries(solidc PRIVATE 
    mimalloc-static
)

# ==============================================================================
# Platform-Specific Compiler Flags & Threading
# ==============================================================================

if(WIN32 OR MINGW)
    # ========== Windows ==========
    target_compile_definitions(solidc PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _USE_MATH_DEFINES
    )

    # Windows uses native threading (no -pthread needed)
    target_link_libraries(solidc PUBLIC ws2_32 advapi32 kernel32)
    
    # Link math library on MinGW
    if(MINGW)
        target_link_libraries(solidc PUBLIC m)
    endif()

    # MSVC-specific flags
    if(MSVC)
        target_compile_options(solidc PRIVATE
            /W4
            /wd4201  # nameless struct/union
            /wd4996  # POSIX name deprecated
            /wd5105  # macro expansion warning in windows headers
        )
        
        # SIMD for MSVC on x64
        if(ARCH_X64)
            target_compile_options(solidc PRIVATE /arch:AVX2)
        endif()
    else()
        # MinGW GCC/Clang on Windows
        target_compile_options(solidc PRIVATE -Wall -Wextra -Wno-unused-function)
        
        # SIMD for x64
        if(ARCH_X64)
            target_compile_options(solidc PRIVATE -mavx2 -msse4.2)
        endif()
    endif()

else()
    # ========== Unix-like (Linux, macOS) ==========
    
    # Common flags for GCC/Clang
    target_compile_options(solidc PRIVATE 
        -Wall 
        -Wextra 
        -Werror 
        -Wno-unused-function
    )
    
    # Threading: Use pthread on Linux, not needed explicitly on macOS
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads REQUIRED)
        target_link_libraries(solidc PUBLIC Threads::Threads)
    endif()
    
    # Math library
    target_link_libraries(solidc PUBLIC m)
    
    # ========== Architecture-Specific SIMD Flags ==========
    if(ARCH_X64)
        # x86-64: AVX2 and SSE4.2
        target_compile_options(solidc PRIVATE 
            -mavx2 
            -msse4.2
        )
        
        # Add tuning for native CPU (only for non-cross-compilation)
        if(NOT CMAKE_CROSSCOMPILING)
            target_compile_options(solidc PRIVATE 
                -mtune=native 
                -march=native
            )
        else()
            # Generic x86-64 baseline for cross-compilation
            target_compile_options(solidc PRIVATE -march=x86-64)
        endif()
        
    elseif(ARCH_ARM64)
        # ARM64: NEON is standard, no special flags needed usually
        # But we can enable specific features if needed
        if(NOT CMAKE_CROSSCOMPILING)
            target_compile_options(solidc PRIVATE -mcpu=native)
        else()
            # Generic ARM64 baseline
            target_compile_options(solidc PRIVATE -march=armv8-a)
        endif()
        
    elseif(ARCH_ARM)
        # ARM 32-bit: Enable NEON if available
        target_compile_options(solidc PRIVATE -mfpu=neon)
    endif()
    
    # ========== Clang-Specific ==========
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(solidc PRIVATE 
            -Wno-gnu-zero-variadic-macro-arguments 
            -fblocks
        )
        
        # BlocksRuntime needed for blocks support on non-Apple Clang
        if(NOT APPLE)
            find_library(BLOCKSRUNTIME_LIB BlocksRuntime)
            if(BLOCKSRUNTIME_LIB)
                target_link_libraries(solidc PUBLIC ${BLOCKSRUNTIME_LIB})
            else()
                message(WARNING "BlocksRuntime not found, blocks may not work")
            endif()
        endif()
    endif()

    # ========== Security Hardening (Release Mode) ==========
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(solidc PRIVATE 
            -fstack-protector-strong 
            -D_FORTIFY_SOURCE=2 
            -fPIE 
            -fPIC
        )
    endif()
endif()

# ==============================================================================
# Installation
# ==============================================================================

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

if(BUILD_BENCHMARKS AND NOT WIN32)
    add_subdirectory(benchmarks)
endif()
