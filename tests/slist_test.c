#include "../include/slist.h"
#include "../include/macros.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NODES 100

void test_basic_push_get(void) {
    slist* l = slist_new(sizeof(size_t));
    ASSERT(l);

    for (size_t i = 0; i < NODES; ++i) {
        slist_push_back(l, &i);
    }
    ASSERT_EQ(slist_size(l), NODES);

    for (size_t i = 0; i < NODES; ++i) {
        size_t* n = slist_get(l, i);
        ASSERT(n);
        ASSERT_EQ(*n, i);
    }

    slist_free(l);
}

/* slist_new must reject zero element sizes (unsafe degenerate lists). */
void test_zero_elem_size_rejected(void) {
    ASSERT(slist_new(0) == NULL);
}

/* Out-of-bounds accessors must be no-ops / NULL, not crashes. */
void test_bounds_checks(void) {
    slist* l = slist_new(sizeof(int));
    ASSERT(l);

    int v = 42;
    ASSERT(slist_get(l, 0) == NULL);   /* empty list */
    slist_push_back(l, &v);
    ASSERT(slist_get(l, 0) != NULL);
    ASSERT(slist_get(l, 1) == NULL);   /* one past end */
    ASSERT(slist_get(NULL, 0) == NULL);

    slist_remove(l, 1);                /* out of range: no-op */
    slist_remove(l, 0);
    ASSERT_EQ(slist_size(l), 0);
    slist_remove(l, 0);                /* empty: no-op */

    slist_insert(l, 1, &v);            /* index > size: no-op */
    ASSERT_EQ(slist_size(l), 0);

    slist_free(l);
    slist_free(NULL);                  /* must be safe */
}

/* index_of compares content (memcmp), and handles NULL safely. */
void test_index_of_content_compare(void) {
    slist* l = slist_new(sizeof(int));
    ASSERT(l);

    int a = 7, b = 8;
    slist_push_back(l, &a);
    slist_push_back(l, &b);
    slist_push_back(l, &a);

    int needle = 8;
    /* A *copy* of the value must still be found: proves content compare. */
    ASSERT_EQ(slist_index_of(l, &needle), 1);
    ASSERT_EQ(slist_index_of(l, &a), 0);
    ASSERT_EQ(slist_index_of(l, NULL), -1);
    ASSERT_EQ(slist_index_of(NULL, &a), -1);

    int missing = 99;
    ASSERT_EQ(slist_index_of(l, &missing), -1);

    slist_free(l);
}

/* insert/remove in the middle keeps size, tail, and ordering consistent. */
void test_mid_insert_remove(void) {
    slist* l = slist_new(sizeof(char));
    ASSERT(l);

    for (char c = 'a'; c < 'e'; c++) slist_push_back(l, &c); /* a b c d */
    char x = 'X';
    slist_insert(l, 2, &x);                                  /* a b X c d */
    ASSERT_EQ(slist_size(l), 5);
    ASSERT_EQ(*(char*)slist_get(l, 2), 'X');
    ASSERT_EQ(*(char*)slist_get(l, 4), 'd');
    ASSERT_EQ(*(char*)l->tail->data, 'd');                   /* tail intact */

    slist_remove(l, 2);                                      /* a b c d */
    ASSERT_EQ(slist_size(l), 4);
    ASSERT_EQ(*(char*)slist_get(l, 2), 'c');
    ASSERT_EQ(*(char*)l->tail->data, 'd');                   /* tail still 'd' */

    /* removing the last element via index must fix the tail pointer */
    slist_remove(l, 3);
    ASSERT_EQ(slist_size(l), 3);
    ASSERT_EQ(*(char*)l->tail->data, 'c');
    /* push after that must still append correctly */
    char y = 'Z';
    slist_push_back(l, &y);
    ASSERT_EQ(*(char*)slist_get(l, 3), 'Z');

    slist_free(l);
}

/* pop_front drains in FIFO order and resets head/tail/size when empty. */
void test_pop_front_order_and_empty(void) {
    slist* l = slist_new(sizeof(int));
    ASSERT(l);

    int vals[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) slist_push_front(l, &vals[i]);

    for (int i = 4; i >= 0; i--) {
        int* front = (int*)slist_get(l, 0);
        ASSERT(front);
        ASSERT_EQ(*front, vals[i]);
        slist_pop_front(l);
    }
    ASSERT_EQ(slist_size(l), 0);
    ASSERT(l->head == NULL);
    ASSERT(l->tail == NULL);
    slist_pop_front(l); /* empty pop: safe no-op */
    ASSERT_EQ(slist_size(l), 0);

    slist_free(l);
}

/* clear on a non-empty list leaves a usable empty list. */
void test_clear(void) {
    slist* l = slist_new(sizeof(double));
    ASSERT(l);

    double d = 1.5;
    for (int i = 0; i < 50; i++) slist_push_back(l, &d);
    slist_clear(l);
    ASSERT_EQ(slist_size(l), 0);
    ASSERT(l->head == NULL);
    ASSERT(l->tail == NULL);
    slist_clear(NULL); /* safe */
    d = 2.5;
    slist_push_back(l, &d);
    ASSERT_EQ(*(double*)slist_get(l, 0), 2.5);

    slist_free(l);
}

/* insert_after / insert_before by value lookup. */
void test_insert_relative(void) {
    slist* l = slist_new(sizeof(int));
    ASSERT(l);

    int v[3] = {1, 2, 3};
    slist_push_back(l, &v[0]);
    slist_push_back(l, &v[2]); /* list: 1 3 */

    int mid = 2, new = 9;
    slist_insert_before(l, &new, &mid); /* target absent: no-op */
    ASSERT_EQ(slist_size(l), 2);

    mid = 3;
    slist_insert_before(l, &new, &mid); /* list: 1 9 3 */
    ASSERT_EQ(slist_size(l), 3);
    ASSERT_EQ(*(int*)slist_get(l, 1), 9);

    int two = 2;
    slist_insert_after(l, &two, &new); /* after value 9 -> 1 9 2 3 */
    ASSERT_EQ(slist_size(l), 4);
    ASSERT_EQ(*(int*)slist_get(l, 2), 2);
    ASSERT_EQ(*(int*)slist_get(l, 3), 3);

    slist_insert_before(l, &new, &v[0]); /* before head -> 9 1 9 2 3 */
    ASSERT_EQ(slist_size(l), 5);
    ASSERT_EQ(*(int*)slist_get(l, 0), 9);

    slist_free(l);
}

/* Large payload near page size exercises aligned_size overflow paths
 * and the max_align_t payload alignment. */
void test_large_payload_alignment(void) {
    enum { PAYLOAD = 4096 };
    slist* l = slist_new(PAYLOAD);
    ASSERT(l);

    unsigned char buf[PAYLOAD];
    memset(buf, 0xAB, sizeof(buf));
    slist_push_back(l, buf);
    slist_push_front(l, buf);

    unsigned char* p = (unsigned char*)slist_get(l, 0);
    ASSERT(p);
    ASSERT(((uintptr_t)p % alignof(max_align_t)) == 0);
    ASSERT(memcmp(p, buf, PAYLOAD) == 0);

    slist_free(l);
}

/* SLIST_FOR_EACH traversal sees every node in order. */
void test_for_each_macro(void) {
    slist* l = slist_new(sizeof(int));
    ASSERT(l);

    enum { N = 10 };
    for (int i = 0; i < N; i++) slist_push_back(l, &i);

    int expected = 0;
    size_t count = 0;
    SLIST_FOR_EACH(l, node) {
        ASSERT_EQ(*(int*)node->data, expected++);
        count++;
    }
    ASSERT_EQ(count, N);

    slist_free(l);
}

int main(void) {
    test_basic_push_get();
    printf("test_basic_push_get passed\n");
    test_zero_elem_size_rejected();
    printf("test_zero_elem_size_rejected passed\n");
    test_bounds_checks();
    printf("test_bounds_checks passed\n");
    test_index_of_content_compare();
    printf("test_index_of_content_compare passed\n");
    test_mid_insert_remove();
    printf("test_mid_insert_remove passed\n");
    test_pop_front_order_and_empty();
    printf("test_pop_front_order_and_empty passed\n");
    test_clear();
    printf("test_clear passed\n");
    test_insert_relative();
    printf("test_insert_relative passed\n");
    test_large_payload_alignment();
    printf("test_large_payload_alignment passed\n");
    test_for_each_macro();
    printf("test_for_each_macro passed\n");
    return 0;
}
