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

// ========== C++ implementation ================
#ifdef __cplusplus
#include <utility>
// Template class using RAII
template <typename F>
struct Deferrer {
    F f;
    Deferrer(F&& f) : f(std::forward<F>(f)) {}
    ~Deferrer() { f(); }
};

#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b) _DEFER_CONCAT_IMPL(a, b)

#define defer(...) auto _DEFER_CONCAT(__defer, __COUNTER__) = Deferrer([&]() { __VA_ARGS__ })

#else

#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b) _DEFER_CONCAT_IMPL(a, b)

#include <stdio.h>
#include <stdlib.h>

#if defined(__GNUC__) && !defined(__clang__)
typedef void (*defer_block)(void);
// GCC/ICC
#define defer_block_create(body)                                                                   \
    ({                                                                                             \
        void __fn__(void) body;                                                                    \
        __fn__;                                                                                    \
    })
#define defer(body)                                                                                \
    defer_block __attribute((cleanup(do_defer))) _DEFER_CONCAT(__defer, __COUNTER__) =             \
        defer_block_create(body)

#elif defined(__clang__)
// Clang/zig cc
typedef void (^defer_block)(void);
#define defer_block_create(body) ^body
#define defer(body)                                                                                \
    defer_block __attribute__((cleanup(do_defer))) _DEFER_CONCAT(__defer, __COUNTER__) =           \
        defer_block_create(body)
#else
#error "Compiler not compatible with defer library"
#endif

__attribute_maybe_unused__ static inline void do_defer(defer_block* ptr) {
    (*ptr)();
}

#endif

// Automatic memory management
// ============================

#ifdef __cplusplus
#include <cstddef>
#include <cstdio>
#include <cstdlib>
extern "C" {
#else
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#if !defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#error "__attribute__(cleanup) is only supported in GCC, Clang and icc & icx compilers."
#endif

#define autofree __attribute__((cleanup(automem_free)))
#define autoclose __attribute__((cleanup(autoclose_file)))
#define autoclean(func) __attribute__((cleanup(func)))

__attribute_maybe_unused__ static inline void automem_free(void* ptr) {
    if (!ptr)
        return;
    void** p = (void**)ptr;
    if (*p) {
        free(*p);
        *p = NULL;
    }
}

__attribute_maybe_unused__ static inline void autoclose_file(void* ptr) {
    if (!ptr)
        return;
    FILE** p = (FILE**)ptr;
    if (*p) {
        fclose(*p);
        *p = NULL;
    }
}

#ifdef __cplusplus
}
#endif

#endif  // __DEFER__H
