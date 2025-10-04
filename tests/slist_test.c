#include "../include/slist.h"
#include "../include/macros.h"

#define NODES 100

int main(void) {
    slist* l = slist_new(sizeof(size_t*));
    ASSERT(l);

    for (size_t i = 0; i < NODES; ++i) {
        slist_push_back(l, &i);
    }

    for (size_t i = 0; i < NODES; ++i) {
        size_t* n = slist_get(l, i);
        ASSERT(n);
        printf("l[%lu]=%lu\n", i, *n);
    }

    slist_free(l);
}
