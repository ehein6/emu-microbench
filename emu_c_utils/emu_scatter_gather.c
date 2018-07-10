/*! \file emu_scatter_gather
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu scatter/gather
 */
#include "emu_scatter_gather.h"
#include <string.h>
#include <cilk/cilk.h>
#include "emu_for_local.h"

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


static void
scatter_tree(void * buffer, long n, long nlet_begin, long nlet_end)
{
    long num_nodelets = nlet_end - nlet_begin;
    // TODO detect this condition before spawning
    if (num_nodelets == 1) { return; }

    long nlet_mid = nlet_begin + (num_nodelets / 2);

    long * local = mw_get_nth(buffer, NODE_ID());
    long * remote = mw_get_nth(buffer, nlet_mid);

//    LOG("nlet[%li]: Copy from %li to %li\n", NODE_ID(), NODE_ID(), nlet_mid);
    // Copy to nlet_mid
    emu_local_for_copy_long(remote, local, n);

//    LOG("nlet[%li]: Spawn scatter_tree(%li - %li)\n", NODE_ID(), nlet_mid, nlet_end);

    // Spawn at target and recurse through my range
    cilk_spawn scatter_tree(remote, n, nlet_mid, nlet_end);
    scatter_tree(local, n, nlet_begin, nlet_mid);
}

void
emu_replicated_array_init(long * array, long n)
{
    scatter_tree(array, n, 0, NODELETS());
}
