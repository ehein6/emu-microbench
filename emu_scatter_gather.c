/*! \file emu_scatter_gather
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu scatter/gather
 */
#include "emu_scatter_gather.h"
#include <string.h>
#include <cilk/cilk.h>

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif


static void
memcpy_long(long* dst, long* src, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

// Copy the contents of an emu_chunked_array to a local array
void
emu_chunked_array_to_local(emu_chunked_array * self, void * local_array)
{
    long elements_per_chunk = 1 << self->log2_elements_per_chunk;
    long longs_per_chunk = (elements_per_chunk * self->element_size) / sizeof(long);

    // Spawn a parallel memcpy at each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long offset = longs_per_chunk * i;
        long * dst_array = ((long*)local_array) + offset;
        long * src_array = (long*)self->data[i];
        cilk_spawn memcpy_long(dst_array, src_array, longs_per_chunk);
    }
}

// Copy the contents of an emu_chunked_array to a local array
void
emu_chunked_array_from_local(emu_chunked_array * self, void * local_array)
{
    long elements_per_chunk = 1 << self->log2_elements_per_chunk;
    long longs_per_chunk = (elements_per_chunk * self->element_size) / sizeof(long);

    // Spawn a parallel memcpy at each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long offset = longs_per_chunk * i;
        long * src_array = ((long*)local_array) + offset;
        long * dst_array = (long*)self->data[i];
        cilk_spawn memcpy_long(dst_array, src_array, longs_per_chunk);
    }
}
