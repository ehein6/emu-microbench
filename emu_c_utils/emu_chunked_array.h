/*! \file emu_chunked_array
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu chunked array struct and access methods
 */
#pragma once

#include <stddef.h>
#include <assert.h>
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

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    declaration = Template("""
        void
        emu_chunked_array_apply_v${num_args}(emu_chunked_array * array, long grain,
            void (*worker)(emu_chunked_array * array, long begin, long end${arg_decls})
            ${arg_decls});
    """)
    cog.out(declaration.substitute(**locals()), dedent=True, trimblanklines=True)

]]]*/
void
emu_chunked_array_apply_v0(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end)
    );
void
emu_chunked_array_apply_v1(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1)
    , void * arg1);
void
emu_chunked_array_apply_v2(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2);
void
emu_chunked_array_apply_v3(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3);
void
emu_chunked_array_apply_v4(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4);
void
emu_chunked_array_apply_v5(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5);
/* [[[end]]] */

/**
 * Initialize each element of the array to @c value
 * @c array must have been initialized to store a long datatype
 * @param array Array to initialize
 * @param value Set each element to this value
 */
void
emu_chunked_array_set_long(emu_chunked_array * array, long value);


/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    declaration = Template("""
        long
        emu_chunked_array_reduce_sum_v${num_args}(emu_chunked_array * array, long grain,
            void (*worker)(emu_chunked_array * array, long begin, long end, long * sum${arg_decls})
            ${arg_decls});
    """)
    cog.out(declaration.substitute(**locals()), dedent=True, trimblanklines=True)

]]]*/
long
emu_chunked_array_reduce_sum_v0(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum)
    );
long
emu_chunked_array_reduce_sum_v1(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1)
    , void * arg1);
long
emu_chunked_array_reduce_sum_v2(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2)
    , void * arg1, void * arg2);
long
emu_chunked_array_reduce_sum_v3(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3);
long
emu_chunked_array_reduce_sum_v4(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4);
long
emu_chunked_array_reduce_sum_v5(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5);
/* [[[end]]] */

/**
 * Return the sum of all elements in the array
 * @c array must have been initialized to store a long datatype
 * @param array Array to sum
 * @return Sum of all array values
 */
long
emu_chunked_array_reduce_sum_long(emu_chunked_array * array);
