/*! \file emu_chunked_array
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu chunked array struct and access methods
 */
#include "emu_chunked_array.h"

#include <cilk/cilk.h>

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#include "emu_c_utils.h"
#include <assert.h>
#include <stdarg.h>

// chunked array type

emu_chunked_array *
emu_chunked_array_replicated_new(long num_elements, long element_size)
{
    emu_chunked_array * rtn = mw_mallocrepl(sizeof(emu_chunked_array));
    assert(rtn);
    emu_chunked_array_replicated_init(rtn, num_elements, element_size);
    return rtn;
}

static long
div_round_up(long num, long den)
{
    assert(den != 0);
    return (num + den - 1) / den;
}

static long
log2_round_up(long x)
{
    assert(x != 0);
    if (x == 1) { return 0; }
    else { return PRIORITY(x - 1) + 1; }
}

static void
emu_chunked_array_init(emu_chunked_array * self, long num_elements, long element_size)
{
    assert(num_elements > 0);
    self->num_elements = num_elements;
    self->element_size = element_size;

    // We will allocate one chunk on each nodelet
    self->num_chunks = NODELETS();

    // How many elements in each chunk?
    long elements_per_chunk = div_round_up(num_elements, self->num_chunks);
    // Round up to nearest power of two
    self->log2_elements_per_chunk = log2_round_up(elements_per_chunk);
    elements_per_chunk = 1 << self->log2_elements_per_chunk;

    // Allocate a chunk on each nodelet
    self->data = mw_malloc2d(self->num_chunks, elements_per_chunk * element_size);
    assert(self->data);
}

void
emu_chunked_array_replicated_init(emu_chunked_array * self, long num_elements, long element_size)
{
    emu_chunked_array *self_0 = mw_get_nth(self, 0);
    emu_chunked_array_init(self_0, num_elements, element_size);
    mw_replicated_init((long*)&self->data, (long)self_0->data);
    mw_replicated_init(&self->element_size, self_0->element_size);
    mw_replicated_init(&self->log2_elements_per_chunk, self_0->log2_elements_per_chunk);
    mw_replicated_init(&self->num_chunks, self_0->num_chunks);
    mw_replicated_init(&self->num_elements, self_0->num_elements);
}

void
emu_chunked_array_replicated_deinit(emu_chunked_array * self)
{
    assert(self->data);
    mw_free(self->data);
    self->data = 0;
    self->num_chunks = 0;
    self->element_size = 0;
    self->log2_elements_per_chunk = 0;
    self->num_elements = 0;
}

void
emu_chunked_array_replicated_free(emu_chunked_array * self)
{
    emu_chunked_array_replicated_deinit(self);
    mw_free(self);
}


long
emu_chunked_array_size(emu_chunked_array * self)
{
    assert(self->data);
    return self->num_elements;
}
