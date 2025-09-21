#ifndef __DEFER__H
#define __DEFER__H

/* ============== COMPILATION===================================================
CLANG/zig cc: clang -Wl,--no-warn-execstack main.c -fblocks -lBlocksRuntime
GCC: clang -Wl,--no-warn-execstack main.c
G++: No special flags required as it uses a special implementaion

If fblocks is not installed:
Ubuntu/debain: sudo apt-get install libblocksruntime-dev
Arch:          sudo pacman -S libdispatch
================================================================================
*/

//******************************************************************************
//*                                                                            *
//*                           C++ IMPLEMENTATION                               *
//*                                                                            *
//******************************************************************************

#include <stddef.h>

#ifdef _WIN32
#define ATTR_UNUSED
#else
#define ATTR_UNUSED __attribute__((__unused__))
#endif

#ifdef __cplusplus
#include <functional>
#include <utility>
// Template class using RAII
template <typename F>
struct Deferrer {
    F f;
    Deferrer(F&& f) : f(std::forward<F>(f)) {}
    ~Deferrer() {
        f();
    }
};

#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b)      _DEFER_CONCAT_IMPL(a, b)

#define defer(block)                                                                               \
    auto _DEFER_CONCAT(__defer, __COUNTER__) = Deferrer<std::function<void()>>([&]() { block; })

#else
//******************************************************************************
//*                                                                            *
//*                           C IMPLEMENTATION                                 *
//*                                                                            *
//******************************************************************************

#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b)      _DEFER_CONCAT_IMPL(a, b)

#if defined(__GNUC__) && !defined(__clang__)
typedef void (*defer_block)(void);
// GCC/ICC
#define defer_block_create(body)                                                                   \
    ({                                                                                             \
        void __fn__(void) body;                                                                    \
        __fn__;                                                                                    \
    })
#define defer(block)                                                                               \
    defer_block __attribute((cleanup(do_defer))) _DEFER_CONCAT(__defer, __COUNTER__) =             \
        defer_block_create(block)

#elif defined(__clang__)
// Clang/zig cc
typedef void (^defer_block)(void);

#define defer_block_create(body) ^body

#define defer(block)                                                                               \
    defer_block __attribute__((cleanup(do_defer))) _DEFER_CONCAT(__defer, __COUNTER__) =           \
        defer_block_create(block)

#elif defined(_MSC_VER)
// MSVC Implementation using try-finally
#define defer(block)                                                                               \
    __try {                                                                                        \
    } __finally {                                                                                  \
        block;                                                                                     \
    }
#else
#error "Compiler not compatible with defer library"
#endif

ATTR_UNUSED static inline void do_defer(defer_block* ptr) {
    if (ptr) (*ptr)();
}

#endif

#endif  // __DEFER__H
