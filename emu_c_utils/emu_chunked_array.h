/*! \file emu_chunked_array
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu chunked array struct and access methods
 */
#pragma once

#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include "emu_grain_helpers.h"

typedef struct emu_chunked_array
{
    // Pointer returned from mw_malloc2D
    void ** data;
    // First argument to mw_malloc2D
    long num_chunks;
    // Second argument to mw_malloc2D
    long element_size;
    // log2() of the number of elements on each chunk (used for indexing)
    long log2_elements_per_chunk;
    // num_elements that was passed to init
    // NOTE: since init rounds up to nearest power of 2,
    // num_chunks * (1<<log2_elements_per_chunk) is usually greater than num_elements
    long num_elements;
} emu_chunked_array;

emu_chunked_array * emu_chunked_array_replicated_new(long num_elements, long element_size);
void emu_chunked_array_replicated_init(emu_chunked_array * self, long num_elements, long element_size);
void emu_chunked_array_replicated_deinit(emu_chunked_array * self);
void emu_chunked_array_replicated_free(emu_chunked_array * self);

static inline void *
emu_chunked_array_index(emu_chunked_array * self, long i)
{
    assert(self->data);

    // Array bounds check
    long elements_per_chunk = 1 << self->log2_elements_per_chunk;
    assert(i < self->num_chunks * elements_per_chunk);

    // First we determine which chunk holds element i
    // i / N
    long chunk = i >> self->log2_elements_per_chunk;

    // Then calculate the position of element i within the chunk
    // i % N
    long mask = elements_per_chunk - 1;
    long offset = i & mask;

    // return data[i / N][i % N]
//    unsigned char * base = (unsigned char*)self->data[chunk];
    unsigned char * base = (unsigned char*)mw_arrayindex(
        (long*)self->data,
        (unsigned long)chunk,
        (unsigned long)self->num_chunks,
        (size_t)elements_per_chunk * self->element_size);
    unsigned char * ptr = base + (offset * self->element_size);
    return ptr;
}

long emu_chunked_array_size(emu_chunked_array * self);

void
emu_chunked_array_apply(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    ...
);

void
emu_chunked_array_apply_var(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    va_list args
);

/**
 * Initialize each element of the array to @c value
 * @c array must have been initialized to store a long datatype
 * @param array Array to initialize
 * @param value Set each element to this value
 */
void
emu_chunked_array_set_long(emu_chunked_array * array, long value);

/**
 * Return the sum of all elements in the array
 * @c array must have been initialized to store a long datatype
 * @param array Array to sum
 * @return Sum of all array values
 */
long
emu_chunked_array_reduce_sum_long(emu_chunked_array * array);
