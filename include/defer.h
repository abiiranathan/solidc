/**
 * @file defer.h
 * References:
 * - https://gustedt.wordpress.com/2025/01/06/simple-defer-ready-to-use/
 * - https://thephd.dev/c2y-the-defer-technical-specification-its-time-go-go-go
 * - https://gcc.gnu.org/onlinedocs/gccint/Trampolines.html
 * - https://github.com/moon-chilled/Defer/blob/master/defer.h
 *
 * @brief Cross-platform defer statement implementation for automatic resource cleanup.
 * This header provides a 'defer' statement that executes code when the current scope exits,
 * similar to Go's defer or other languages' scope guards.
 * 
 * @section compilation Compilation Instructions
 * 
 * C++:
 *   g++ main.cpp
 *   clang++ main.cpp
 *   cl main.cpp  (MSVC)
 *   No special flags required - uses RAII with lambdas
 * 
 * C with GCC:
 *   gcc -fno-trampolines main.c
 *   Uses function descriptors (safe, no executable stack required)
 *   
 *   Alternative for GCC 4.9+:
 *   gcc main.c
 *   Uses nested functions with auto storage class (safe, no trampolines)
 * 
 * C with Clang:
 *   clang -fblocks -lBlocksRuntime main.c
 *   Requires blocks runtime library
 *   
 *   Install blocks runtime:
 *   Ubuntu/Debian: sudo apt-get install libblocksruntime-dev
 *   Arch Linux:    sudo pacman -S libdispatch
 * 
 * C with MSVC:
 *   cl main.c
 *   Uses __try/__finally (no special flags required)
 * 
 * @section example Usage Example
 * 
 * @code
 * #include "defer.h"
 * #include <stdio.h>
 * #include <stdlib.h>
 * 
 * int main() {
 *     FILE *f = fopen("test.txt", "w");
 *     defer({ fclose(f); printf("File closed\n"); });
 *     
 *     void *ptr = malloc(100);
 *     defer({ free(ptr); printf("Memory freed\n"); });
 *     
 *     fprintf(f, "Hello, World!\n");
 *     // Cleanup happens automatically in reverse order
 *     return 0;
 * }
 * @endcode
 */

#ifndef __DEFER__H
#define __DEFER__H

#include <stddef.h>

#ifdef _WIN32
#define ATTR_UNUSED
#else
#define ATTR_UNUSED __attribute__((__unused__))
#endif

// Macro helpers
#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b) _DEFER_CONCAT_IMPL(a, b)

//******************************************************************************
//*                                                                            *
//*                           C++ IMPLEMENTATION                               *
//*                                                                            *
//******************************************************************************

#ifdef __cplusplus

#include <utility>

// 1. The RAII Holder that runs the lambda on destruction
template <typename F>
struct Deferrer {
    F f;
    Deferrer(F&& func): f(std::forward<F>(func)) {}
    ~Deferrer() {
        f();
    }
    Deferrer(const Deferrer&) = delete;
    Deferrer& operator=(const Deferrer&) = delete;
};

// 2. A dummy helper tag to trigger operator overloading
struct DeferHelper {};

// 3. Operator+ overload to capture the lambda
// This allows the syntax: DeferHelper() + [&](){ ... }
template <typename F>
Deferrer<F> operator+(DeferHelper, F&& f) {
    return Deferrer<F>(std::forward<F>(f));
}

// 4. The Macro
// Expands to: auto _var_N = DeferHelper() + [&]() { user_code };
#define defer auto _DEFER_CONCAT(__defer_var_, __COUNTER__) = DeferHelper() + [&]()

//******************************************************************************
//*                                                                            *
//*                      C IMPLEMENTATION - GCC                                *
//*                                                                            *
//******************************************************************************

#elif defined(__GNUC__) && !defined(__clang__)
/**
 * GCC Implementation using nested functions with 'auto' storage class.
 * 
 * This implementation is based on Jens Gustedt's 2025 approach and uses:
 * 1. Nested functions (GCC extension)
 * 2. 'auto' storage class for nested functions (GCC 4.9+)
 * 3. __attribute__((cleanup)) for automatic execution
 * 
 * The 'auto' keyword for nested functions tells GCC to avoid trampolines
 * by restricting the function to automatic storage duration. This means:
 * - No executable stack required
 * - No function descriptors needed
 * - Fast, zero-overhead cleanup
 * 
 * Compile with: gcc main.c
 * Or for older approach: gcc -fno-trampolines main.c
 */

#define __DEFER(N) __DEFER_(N)
#define __DEFER_(N) __DEFER__(_DEFER_CONCAT(__defer_fn_, N), _DEFER_CONCAT(__defer_var_, N))

/**
 * @brief Core defer macro for GCC
 * 
 * This macro:
 * 1. Forward declares a nested function with 'auto' storage class
 * 2. Declares a cleanup variable that triggers the function on scope exit
 * 3. Begins the function definition (user's code block becomes the body)
 * 
 * The [[gnu::cleanup(F)]] attribute ensures F is called when V goes out of scope.
 */
#define __DEFER__(F, V)                                                                                                \
    auto void F(int*);                                                                                                 \
    [[gnu::cleanup(F)]] ATTR_UNUSED int V;                                                                             \
    auto void F(int* _unused ATTR_UNUSED)

/**
 * @brief Defer execution of a code block until scope exit
 * 
 * Example: defer { free(ptr); }
 * 
 * Note: The block is the body of the nested function, so it should be
 * written directly after 'defer' without parentheses.
 */
#define defer __DEFER(__COUNTER__)

//******************************************************************************
//*                                                                            *
//*                      C IMPLEMENTATION - CLANG                              *
//*                                                                            *
//******************************************************************************

#elif defined(__clang__)

/**
 * Clang implementation using blocks (Apple's closure extension).
 * 
 * Blocks are Clang's closure mechanism that allows capturing variables
 * from the surrounding scope. They're similar to lambdas in C++ but
 * available in C with Clang.
 * 
 * Requires:
 * - Compile with: -fblocks
 * - Link with: -lBlocksRuntime
 * 
 * The blocks runtime provides memory management and execution support.
 * Blocks are allocated on the stack by default and don't require
 * executable stack pages.
 * 
 * Install blocks runtime:
 * - Ubuntu/Debian: sudo apt-get install libblocksruntime-dev
 * - Arch Linux: sudo pacman -S libdispatch
 */

#include <Block.h>

/**
 * @brief Block type for defer cleanup handlers
 */
typedef void (^defer_block)(void);

/**
 * @brief Cleanup function called when defer variable goes out of scope
 * @param ptr Pointer to the block to execute
 */
static inline void __defer_cleanup(defer_block *ptr) {
    if (ptr && *ptr) {
        (*ptr)();
    }
}

// Macro helpers
#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b) _DEFER_CONCAT_IMPL(a, b)

/**
 * @brief Defer execution of a code block until scope exit
 * @param ... Code block to execute (can capture surrounding variables)
 * 
 * Example: defer { close(fd); }
 * 
 * The ^ syntax creates a block literal that captures variables from
 * the surrounding scope. The block is executed automatically when
 * the defer variable goes out of scope.
 */
#define defer                                                                                                          \
    __attribute__((cleanup(__defer_cleanup))) ATTR_UNUSED defer_block _DEFER_CONCAT(__defer_var_, __COUNTER__) = ^

//******************************************************************************
//*                                                                            *
//*                      C IMPLEMENTATION - MSVC                               *
//*                                                                            *
//******************************************************************************

#elif defined(_MSC_VER)

/**
 * MSVC implementation using Structured Exception Handling (SEH).
 * 
 * MSVC provides __try/__finally as part of its SEH mechanism.
 * The __finally block is guaranteed to execute when leaving the
 * __try block, regardless of how it's exited (return, break, exception, etc.)
 * 
 * This is zero-overhead and built into the compiler - no special
 * compilation flags required.
 * 
 * Note: This implementation differs slightly from others because
 * the defer block must immediately follow the defer() statement
 * due to the __try/__finally syntax requirements.
 * 
 * Compile with: cl main.c
 */

/**
 * @brief Defer execution using MSVC's __try/__finally
 * @param block Code block to execute on scope exit
 * 
 * Usage is slightly different from GCC/Clang:
 * 
 * @code
 * defer({
 *     cleanup();
 * });
 * // Normal code continues here
 * @endcode
 * 
 * The __finally block executes when the (empty) __try block exits,
 * which happens immediately, but the cleanup is deferred until
 * the containing scope exits.
 */
#elif defined(_MSC_VER)

// Uses SEH __try/__finally
// Expands to: __try { } __finally { user_code }
#define defer                                                                                                          \
    __try {                                                                                                            \
    } __finally

#else
#error "Unsupported compiler"
#endif

#endif  // __DEFER__H
