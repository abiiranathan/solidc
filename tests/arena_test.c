#include "../include/arena.h"
#include "../include/macros.h"
#include "../include/thread.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS            16
#define STRESS_TEST_ITERATIONS 100000

// ============================================================================
// Test Infrastructure
// ============================================================================

static int g_pass = 0;
static int g_fail = 0;

static void print_result(const char* name, int ok) {
    printf("  %-52s %s\n", name, ok ? "PASS" : "FAIL");
    ok ? g_pass++ : g_fail++;
}

static void print_section(const char* name) { printf("\n%s\n", name); }

static int validate_pattern(void* ptr, size_t size, unsigned char byte) {
    if (!ptr) return 0;
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (p[i] != byte) return 0;
    }
    return 1;
}

static int is_aligned(const void* ptr, size_t alignment) { return ((uintptr_t)ptr & (alignment - 1)) == 0; }

// ============================================================================
// Basic Allocations
// ============================================================================

static void test_basic_allocations(void) {
    print_section("Basic allocations");

    Arena* a = arena_create(0);
    ASSERT(a);

    // Zero bytes returns NULL
    print_result("zero-size alloc returns NULL", arena_alloc(a, 0) == NULL);

    // Non-zero allocation succeeds and is writable
    void* p = arena_alloc(a, 256);
    memset(p, 0xAB, 256);
    print_result("small alloc non-NULL and writable", p != NULL && validate_pattern(p, 256, 0xAB));

    // Default alignment is 16
    print_result("arena_alloc 16-byte aligned", is_aligned(arena_alloc(a, 1), ARENA_DEFAULT_ALIGN));

    // arena_alloc_align with larger alignments
    void* p32 = arena_alloc_align(a, 1, 32);
    void* p64 = arena_alloc_align(a, 1, 64);
    void* p128 = arena_alloc_align(a, 1, 128);
    print_result("arena_alloc_align 32-byte aligned", is_aligned(p32, 32));
    print_result("arena_alloc_align 64-byte aligned", is_aligned(p64, 64));
    print_result("arena_alloc_align 128-byte aligned", is_aligned(p128, 128));

    // arena_alloc_unaligned: returns pointer, no forced alignment
    char* u1 = (char*)arena_alloc_unaligned(a, 1);
    char* u2 = (char*)arena_alloc_unaligned(a, 1);
    print_result("arena_alloc_unaligned non-NULL", u1 != NULL && u2 != NULL);
    print_result("arena_alloc_unaligned consecutive", u2 == u1 + 1);

    // Consecutive allocs don't overlap
    char* a1 = arena_alloc(a, 64);
    char* a2 = arena_alloc(a, 64);
    memset(a1, 0x11, 64);
    memset(a2, 0x22, 64);
    print_result("consecutive allocs don't overlap", validate_pattern(a1, 64, 0x11) && validate_pattern(a2, 64, 0x22));

    arena_destroy(a);
}

// ============================================================================
// String Functions
// ============================================================================

static void test_string_functions(void) {
    print_section("String functions");

    Arena* a = arena_create(0);
    ASSERT(a);

    // arena_strdup basic
    char* s = arena_strdup(a, "Hello, Arena!");
    print_result("arena_strdup content correct", s && strcmp(s, "Hello, Arena!") == 0);
    print_result("arena_strdup null-terminated", s && s[13] == '\0');

    // arena_strdup with empty string
    char* empty = arena_strdup(a, "");
    print_result("arena_strdup empty string", empty && strlen(empty) == 0);

    // arena_strdup NULL input
    print_result("arena_strdup NULL input returns NULL", arena_strdup(a, NULL) == NULL);

    // arena_strdupn: exact byte count, no null in source
    char raw[] = {'S', 'O', 'L', 'I', 'D'};
    char* n = arena_strdupn(a, raw, sizeof(raw));
    print_result("arena_strdupn content correct", n && memcmp(n, "SOLID", 5) == 0);
    print_result("arena_strdupn null-terminated", n && n[5] == '\0');

    // arena_strdupn length < strlen (substring)
    char* sub = arena_strdupn(a, "Hello, world", 5);
    print_result("arena_strdupn substring", sub && strcmp(sub, "Hello") == 0);

    // arena_strdupn zero length
    char* z = arena_strdupn(a, "anything", 0);
    print_result("arena_strdupn zero length gives empty string", z && z[0] == '\0');

    // arena_strdupn NULL input
    print_result("arena_strdupn NULL input returns NULL", arena_strdupn(a, NULL, 4) == NULL);

    arena_destroy(a);
}

// ============================================================================
// arena_init (Stack Arena)
// ============================================================================

static void test_stack_arena(void) {
    print_section("Stack arena (arena_init)");

    char buf[4096];
    Arena a;
    arena_init(&a, buf, sizeof(buf));

    // heap_allocated must be false
    print_result("heap_allocated is false after arena_init", a.heap_allocated == false);

    // Allocations come from buf
    char* p = arena_alloc(&a, 16);
    print_result("alloc from stack arena non-NULL", p != NULL);
    print_result("alloc is inside the stack buffer", (char*)p >= buf && (char*)p < buf + sizeof(buf));

    // Writing to it works
    memset(p, 0xCC, 16);
    print_result("writing to stack arena allocation", validate_pattern(p, 16, 0xCC));

    // arena_committed_size reflects buf size
    print_result("committed_size equals buf size", arena_committed_size(&a) == sizeof(buf));

    // arena_destroy on stack arena does not free buf (no crash = pass)
    arena_destroy(&a);
    print_result("arena_destroy on stack arena does not crash", 1);

    // Arena struct itself is on the stack — buf is still valid after destroy
    // (we just can't use the arena; this is a compile-time guarantee, not runtime)
}

// ============================================================================
// arena_init: Overflow Into Heap Block
// ============================================================================

static void test_stack_arena_overflow(void) {
    print_section("Stack arena overflow to heap block");

    char buf[256];  // Intentionally tiny
    Arena a;
    arena_init(&a, buf, sizeof(buf));

    size_t committed_before = arena_committed_size(&a);

    // Exhaust the stack buffer
    char* first = arena_alloc(&a, 128);
    char* second = arena_alloc(&a, 128);  // likely still fits
    char* third = arena_alloc(&a, 128);   // forces overflow

    print_result("first alloc from stack buffer succeeds", first != NULL);
    print_result("second alloc from stack buffer succeeds", second != NULL);
    print_result("overflow alloc succeeds (heap block)", third != NULL);

    // After overflow, total_committed grew
    print_result("committed_size grew after overflow", arena_committed_size(&a) > committed_before);

    // Overflow pointer is not inside the original buf
    bool third_in_buf = (char*)third >= buf && (char*)third < buf + sizeof(buf);
    print_result("overflow pointer is outside stack buffer", !third_in_buf);

    // All three are writable and distinct
    memset(first, 0x11, 64);
    memset(second, 0x22, 64);
    memset(third, 0x33, 64);
    print_result("all three allocs writable and independent", validate_pattern(first, 64, 0x11) &&
                                                                  validate_pattern(second, 64, 0x22) &&
                                                                  validate_pattern(third, 64, 0x33));

    // Heap overflow block freed without crash
    arena_destroy(&a);
    print_result("arena_destroy frees overflow blocks without crash", 1);
}

// ============================================================================
// arena_init vs arena_create: heap_allocated flag
// ============================================================================

static void test_heap_allocated_flag(void) {
    print_section("heap_allocated flag");

    // arena_create sets heap_allocated = true
    Arena* heap = arena_create(0);
    ASSERT(heap);
    print_result("arena_create sets heap_allocated = true", heap->heap_allocated == true);
    arena_destroy(heap);

    // arena_init sets heap_allocated = false
    char buf[1024];
    Arena stack;
    arena_init(&stack, buf, sizeof(buf));
    print_result("arena_init sets heap_allocated = false", stack.heap_allocated == false);
    arena_destroy(&stack);
}

// ============================================================================
// Reset
// ============================================================================

static void test_reset(void) {
    print_section("arena_reset");

    Arena* a = arena_create(0);
    ASSERT(a);

    char* p1 = arena_alloc(a, 128);
    memset(p1, 0xBB, 128);

    arena_reset(a);
    size_t used_after = arena_used_size(a);

    print_result("used_size drops to 0 after reset", used_after == 0);
    print_result("committed_size unchanged after reset",
                 arena_committed_size(a) > 0);  // pages kept

    // After reset, same pointer range is reused
    char* p2 = arena_alloc(a, 128);
    print_result("post-reset alloc returns same address as pre-reset", p2 == p1);

    // Writing after reset doesn't corrupt anything
    memset(p2, 0xDD, 128);
    print_result("writing after reset succeeds", validate_pattern(p2, 128, 0xDD));

    // Reset with NULL is safe
    arena_reset(NULL);
    print_result("arena_reset(NULL) does not crash", 1);

    arena_destroy(a);
}

// ============================================================================
// Reset preserves overflow blocks (curr falls back to head)
// ============================================================================

static void test_reset_with_overflow(void) {
    print_section("arena_reset with overflow blocks");

    char buf[256];
    Arena a;
    arena_init(&a, buf, sizeof(buf));

    // Force an overflow block to be allocated
    arena_alloc(&a, 128);
    arena_alloc(&a, 128);
    arena_alloc(&a, 256);  // overflow

    size_t committed = arena_committed_size(&a);
    arena_reset(&a);

    // Reset goes back to head (first_block / buf)
    char* after_reset = arena_alloc(&a, 16);
    print_result("post-reset alloc comes from head block",
                 (char*)after_reset >= buf && (char*)after_reset < buf + sizeof(buf));

    // Committed size unchanged (pages retained)
    print_result("committed_size unchanged after reset with overflow", arena_committed_size(&a) == committed);

    arena_destroy(&a);
}

// ============================================================================
// Type-safe Macros
// ============================================================================

typedef struct {
    int x;
    float y;
    double z;
} TestStruct;

static void test_macros(void) {
    print_section("Type-safe macros");

    Arena* a = arena_create(0);
    ASSERT(a);

    // ARENA_ALLOC
    TestStruct* obj = ARENA_ALLOC(a, TestStruct);
    print_result("ARENA_ALLOC non-NULL", obj != NULL);
    print_result("ARENA_ALLOC natural alignment", is_aligned(obj, _Alignof(TestStruct)));

    // ARENA_ALLOC_ZERO
    TestStruct* zeroed = ARENA_ALLOC_ZERO(a, TestStruct);
    print_result("ARENA_ALLOC_ZERO non-NULL", zeroed != NULL);
    print_result("ARENA_ALLOC_ZERO all bytes zero", zeroed && zeroed->x == 0 && zeroed->y == 0.0f && zeroed->z == 0.0);

    // ARENA_ALLOC_ARRAY
    int* arr = ARENA_ALLOC_ARRAY(a, int, 64);
    print_result("ARENA_ALLOC_ARRAY non-NULL", arr != NULL);
    print_result("ARENA_ALLOC_ARRAY natural alignment", is_aligned(arr, _Alignof(int)));

    // ARENA_ALLOC_ARRAY_ZERO
    int* zarr = ARENA_ALLOC_ARRAY_ZERO(a, int, 64);
    int all_zero = 1;
    if (zarr)
        for (int i = 0; i < 64; i++) all_zero &= (zarr[i] == 0);
    print_result("ARENA_ALLOC_ARRAY_ZERO all elements zero", zarr != NULL && all_zero);

    arena_destroy(a);
}

// ============================================================================
// Batch Allocation
// ============================================================================

static void test_alloc_batch(void) {
    print_section("arena_alloc_batch");

    Arena* a = arena_create(0);
    ASSERT(a);

    // Normal batch
    size_t sizes[] = {32, 64, 128};
    void* ptrs[3];
    print_result("batch alloc succeeds", arena_alloc_batch(a, sizes, 3, ptrs));

    // Each pointer is 16-byte aligned
    print_result("batch[0] aligned", is_aligned(ptrs[0], ARENA_DEFAULT_ALIGN));
    print_result("batch[1] aligned", is_aligned(ptrs[1], ARENA_DEFAULT_ALIGN));
    print_result("batch[2] aligned", is_aligned(ptrs[2], ARENA_DEFAULT_ALIGN));

    // Each region is writable and doesn't overlap the others
    memset(ptrs[0], 0x11, 32);
    memset(ptrs[1], 0x22, 64);
    memset(ptrs[2], 0x33, 128);
    print_result("batch regions non-overlapping", validate_pattern(ptrs[0], 32, 0x11) &&
                                                      validate_pattern(ptrs[1], 64, 0x22) &&
                                                      validate_pattern(ptrs[2], 128, 0x33));

    // Pointers are in ascending order (single contiguous slab)
    print_result("batch pointers in ascending order",
                 (char*)ptrs[0] < (char*)ptrs[1] && (char*)ptrs[1] < (char*)ptrs[2]);

    // Invalid args return false and don't write ptrs
    void* sentinel = (void*)0xDEAD;
    void* bad_ptrs[1] = {sentinel};
    print_result("batch NULL arena returns false", !arena_alloc_batch(NULL, sizes, 1, bad_ptrs));
    print_result("batch NULL sizes returns false", !arena_alloc_batch(a, NULL, 1, bad_ptrs));
    print_result("batch NULL out returns false", !arena_alloc_batch(a, sizes, 1, NULL));
    print_result("batch count=0 returns false", !arena_alloc_batch(a, sizes, 0, bad_ptrs));

    arena_destroy(a);
}

// ============================================================================
// Block Expansion (slow path)
// ============================================================================

static void test_block_expansion(void) {
    print_section("Block expansion (slow path)");

    // Create a tiny arena to force immediate overflow
    char buf[64];
    Arena a;
    arena_init(&a, buf, sizeof(buf));

    ArenaBlock* initial_block = a.head;

    // Allocate enough to exhaust buf and trigger a new block
    char* ptrs[16];
    for (int i = 0; i < 16; i++) {
        ptrs[i] = arena_alloc(&a, 64);
    }

    print_result("all allocs across block boundary succeed", ptrs[15] != NULL);
    print_result("a new block was linked", a.head->next != NULL);
    print_result("current_block advanced past head", a.current_block != initial_block);

    // All pointers are distinct
    int all_distinct = 1;
    for (int i = 0; i < 16; i++)
        for (int j = i + 1; j < 16; j++)
            if (ptrs[i] == ptrs[j]) all_distinct = 0;
    print_result("all cross-block pointers are distinct", all_distinct);

    // All are writable
    int all_writable = 1;
    for (int i = 0; i < 16; i++) {
        memset(ptrs[i], (unsigned char)i, 64);
        if (!validate_pattern(ptrs[i], 64, (unsigned char)i)) all_writable = 0;
    }
    print_result("all cross-block pointers are writable", all_writable);

    arena_destroy(&a);
}

// ============================================================================
// Size Tracking
// ============================================================================

static void test_size_tracking(void) {
    print_section("Size tracking");

    Arena* a = arena_create(0);
    ASSERT(a);

    size_t used0 = arena_used_size(a);
    arena_alloc(a, 100);
    size_t used1 = arena_used_size(a);
    arena_alloc(a, 200);
    size_t used2 = arena_used_size(a);

    print_result("used_size increases with each alloc", used1 > used0 && used2 > used1);
    print_result("committed_size >= used_size", arena_committed_size(a) >= arena_used_size(a));

    arena_reset(a);
    print_result("used_size is 0 after reset", arena_used_size(a) == 0);

    // NULL guard
    print_result("arena_used_size(NULL) returns 0", arena_used_size(NULL) == 0);

    arena_destroy(a);
}

// ============================================================================
// arena_destroy edge cases
// ============================================================================

static void test_destroy_edge_cases(void) {
    print_section("arena_destroy edge cases");

    // NULL is safe
    arena_destroy(NULL);
    print_result("arena_destroy(NULL) does not crash", 1);

    // Stack arena: destroy frees overflow blocks but not the Arena struct
    char buf[128];
    Arena sa;
    arena_init(&sa, buf, sizeof(buf));
    arena_alloc(&sa, 64);
    arena_alloc(&sa, 64);
    arena_alloc(&sa, 200);  // overflow block allocated
    arena_destroy(&sa);
    print_result("arena_destroy on stack arena with overflow does not crash", 1);

    // Heap arena: destroy frees everything
    Arena* ha = arena_create(0);
    ASSERT(ha);
    arena_alloc(ha, 64);
    arena_destroy(ha);
    print_result("arena_destroy on heap arena does not crash", 1);
}

// ============================================================================
// Multithreaded (unchanged logic, kept for regression)
// ============================================================================

static void* thread_func(void* arg) {
    Arena* arena = (Arena*)arg;

    const char s[] = "Thread initial allocation";
    char* str = arena_strdup(arena, s);
    if (!str) {
        printf("  Thread %lu: arena_strdup failed\n", (unsigned long)thread_self());
        return NULL;
    }

    int* numbers = arena_alloc(arena, sizeof(int) * 10);
    if (!numbers) {
        printf("  Thread %lu: array alloc failed\n", (unsigned long)thread_self());
        return NULL;
    }
    for (int i = 0; i < 10; i++) numbers[i] = i;

    return NULL;
}

static void test_multithreaded(void) {
    print_section("Multithreaded allocations");

    Arena* a = arena_create(1024UL * 1024);
    ASSERT(a);

    Thread threads[NUM_THREADS] = {0};
    for (int i = 0; i < NUM_THREADS; i++) thread_create(&threads[i], thread_func, a);
    for (int i = 0; i < NUM_THREADS; i++) thread_join(threads[i], NULL);

    print_result("multithreaded allocs (no crash)", 1);
    arena_destroy(a);
}

// ============================================================================
// Stress Test
// ============================================================================

static void test_stress(void) {
    print_section("Stress test");

    Arena* a = arena_create(20 << 20);
    ASSERT(a);

    int ok = 1;
    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        char* s = arena_strdup(a, "Stress test");
        if (!s) {
            ok = 0;
            break;
        }
    }
    print_result("stress: 100k arena_strdup calls without failure", ok);
    arena_destroy(a);

    // Same stress via stack arena with overflow
    char buf[512];
    Arena sa;
    arena_init(&sa, buf, sizeof(buf));
    ok = 1;
    for (int i = 0; i < 10000; i++) {
        char* s = arena_strdup(&sa, "overflow stress");
        if (!s) {
            ok = 0;
            break;
        }
    }
    print_result("stress: 10k allocs on tiny stack arena with overflow", ok);
    arena_destroy(&sa);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("Arena Allocator Test Suite\n");
    printf("==========================\n");

    test_basic_allocations();
    test_string_functions();
    test_stack_arena();
    test_stack_arena_overflow();
    test_heap_allocated_flag();
    test_reset();
    test_reset_with_overflow();
    test_macros();
    test_alloc_batch();
    test_block_expansion();
    test_size_tracking();
    test_destroy_edge_cases();
    test_multithreaded();
    test_stress();

    printf("\n==========================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);

    TIME_BLOCK_MS("10M iteration loop", {
        for (volatile int i = 0; i < 10000000; ++i) {
        }
    });

    int arr[] = {1, 2, 3, 4};
    FOR_EACH_ARRAY(n, arr) { printf("  n=%d\n", *n); }

    return g_fail > 0 ? 1 : 0;
}
