#include "../include/dynarray.h"
#include <stdio.h>   // for fprintf, stderr
#include <stdlib.h>  // for malloc, realloc, free
#include <string.h>  // for memcpy, memmove

/**
 * Calculates the new capacity with overflow checking.
 * @param current Current capacity.
 * @param element_size Size of each element.
 * @return New capacity, or 0 on overflow.
 */
static inline size_t calculate_growth(size_t current, size_t element_size) {
    // Check for potential overflow in growth calculation
    if (current > SIZE_MAX / DYNARRAY_GROWTH_NUMERATOR) {
        return 0;  // Overflow would occur
    }

    size_t new_capacity = (current * DYNARRAY_GROWTH_NUMERATOR) / DYNARRAY_GROWTH_DENOMINATOR;

    // Ensure we actually grow (handle rounding down)
    if (new_capacity <= current) {
        new_capacity = current + 1;
    }

    // Check if allocation would overflow
    if (new_capacity > SIZE_MAX / element_size) {
        return 0;  // Would overflow
    }

    return new_capacity;
}

bool dynarray_init(dynarray_t* arr, size_t element_size, size_t initial_capacity) {
    if (arr == NULL || element_size == 0) {
        return false;
    }

    if (initial_capacity == 0) {
        initial_capacity = DYNARRAY_INITIAL_CAPACITY;
    }

    // Check for potential overflow
    if (initial_capacity > SIZE_MAX / element_size) {
        return false;
    }

    void* data = malloc(element_size * initial_capacity);
    if (data == NULL) {
        return false;
    }

    *arr = (dynarray_t){
        .data         = data,
        .size         = 0,
        .capacity     = initial_capacity,
        .element_size = element_size,
    };

    return true;
}

void dynarray_free(dynarray_t* arr) {
    if (arr == NULL) {
        return;
    }

    free(arr->data);
    *arr = (dynarray_t){0};  // Zero out the structure
}

bool dynarray_push(dynarray_t* arr, const void* element) {
    if (arr == NULL || element == NULL) {
        return false;
    }

    // Check if we need to grow
    if (arr->size >= arr->capacity) {
        size_t new_capacity = calculate_growth(arr->capacity, arr->element_size);
        if (new_capacity == 0) {
            return false;
        }

        if (!dynarray_reserve(arr, new_capacity)) {
            return false;
        }
    }

    // Copy element to the end
    unsigned char* dest = (unsigned char*)arr->data + (arr->size * arr->element_size);
    memcpy(dest, element, arr->element_size);
    arr->size++;

    return true;
}

bool dynarray_pop(dynarray_t* arr, void* out_element) {
    if (arr == NULL || arr->size == 0) {
        return false;
    }

    arr->size--;

    // Copy element if requested
    if (out_element != NULL) {
        const unsigned char* src = (const unsigned char*)arr->data + (arr->size * arr->element_size);
        memcpy(out_element, src, arr->element_size);
    }

    // Shrink if usage drops below threshold and we have significant unused capacity
    if (arr->capacity > DYNARRAY_INITIAL_CAPACITY && arr->size < arr->capacity / DYNARRAY_SHRINK_THRESHOLD) {
        size_t new_capacity = arr->capacity / DYNARRAY_GROWTH_DENOMINATOR;
        if (new_capacity < DYNARRAY_INITIAL_CAPACITY) {
            new_capacity = DYNARRAY_INITIAL_CAPACITY;
        }

        // Ignore failure - shrinking is optional optimization
        (void)dynarray_reserve(arr, new_capacity);
    }

    return true;
}

void* dynarray_get(const dynarray_t* arr, size_t index) {
    if (arr == NULL || index >= arr->size) {
        return NULL;
    }
    return (unsigned char*)arr->data + (index * arr->element_size);
}

bool dynarray_set(dynarray_t* arr, size_t index, const void* element) {
    if (arr == NULL || element == NULL || index >= arr->size) {
        return false;
    }

    unsigned char* dest = (unsigned char*)arr->data + (index * arr->element_size);
    memcpy(dest, element, arr->element_size);

    return true;
}

bool dynarray_reserve(dynarray_t* arr, size_t new_capacity) {
    if (arr == NULL) {
        return false;
    }

    // Don't shrink below current size
    if (new_capacity < arr->size) {
        new_capacity = arr->size;
    }

    // No-op if already at desired capacity
    if (new_capacity == arr->capacity) {
        return true;
    }

    // Check for overflow
    if (new_capacity > SIZE_MAX / arr->element_size) {
        return false;
    }

    void* new_data = realloc(arr->data, new_capacity * arr->element_size);
    if (new_data == NULL && new_capacity > 0) {
        return false;
    }

    arr->data     = new_data;
    arr->capacity = new_capacity;

    return true;
}

bool dynarray_shrink_to_fit(dynarray_t* arr) {
    if (arr == NULL) {
        return false;
    }

    // Ensure we keep at least the initial capacity to avoid degenerate behavior
    size_t target_capacity = arr->size > DYNARRAY_INITIAL_CAPACITY ? arr->size : DYNARRAY_INITIAL_CAPACITY;

    return dynarray_reserve(arr, target_capacity);
}

void dynarray_clear(dynarray_t* arr) {
    if (arr == NULL) {
        return;
    }

    arr->size = 0;
    // Note: We deliberately don't free memory here - capacity remains unchanged
}
