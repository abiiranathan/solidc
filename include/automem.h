#ifndef _SOLIDC_AUTO_MEM_H
#define _SOLIDC_AUTO_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// Auto free memory
#define autofree __attribute__((cleanup(automem_free)))

// Auto close file
#define autoclose __attribute__((cleanup(autoclose_file)))

// Auto close and remove file
#define autormfile __attribute__((cleanup(autorm_file)))

#ifdef AUTOMEM_IMPL
#include <stdio.h>
#include <stdlib.h>

static inline void automem_free(void* ptr) {
    if (!ptr)
        return;
    void** p = (void**)ptr;
    if (*p) {
        free(*p);
        *p = NULL;
    }
}

static inline void autoclose_file(void* ptr) {
    if (!ptr)
        return;
    FILE** p = (FILE**)ptr;
    if (*p) {
        fclose(*p);
        *p = NULL;
    }
}

static inline void autorm_file(void* ptr) {
    if (!ptr)
        return;
    FILE** p = (char**)ptr;
    if (*p) {
        fclose(*p);
        remove(*p);
        *p = NULL;
    }
}

#endif  // AUTOMEM_IMPL

#ifdef __cplusplus
}
#endif

#endif  // _SOLIDC_AUTO_MEM_H
