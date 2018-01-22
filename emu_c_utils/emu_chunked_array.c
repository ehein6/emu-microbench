#include "emu_chunked_array.h"

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#include <assert.h>
#include <stdlib.h>


// chunked array type

emu_chunked_array *
emu_chunked_array_new(size_t num_elements, size_t element_size)
{
    emu_chunked_array * rtn = calloc(sizeof(emu_chunked_array), 1);
    assert(rtn);
    emu_chunked_array_init(rtn, num_elements, element_size);
    return rtn;
}

static size_t
div_round_up(size_t num, size_t den)
{
    assert(den != 0);
    return (num + den - 1) / den;
}

static size_t
log2_round_up(size_t x)
{
    assert(x != 0);
    if (x == 1) { return 0; }
    else { return PRIORITY(x - 1) + 1; }
}

void
emu_chunked_array_init(emu_chunked_array * self, size_t num_elements, size_t element_size)
{
    assert(num_elements > 0);
    self->num_elements = num_elements;
    self->element_size = element_size;

    // We will allocate one chunk on each nodelet
    self->num_chunks = NODELETS();

    // How many elements in each chunk?
    size_t elements_per_chunk = div_round_up(num_elements, self->num_chunks);
    // Round up to nearest power of two
    self->log2_elements_per_chunk = log2_round_up(elements_per_chunk);
    elements_per_chunk = 1 << self->log2_elements_per_chunk;

    // Allocate a chunk on each nodelet
    self->data = mw_malloc2d(self->num_chunks, elements_per_chunk * element_size);
    assert(self->data);
}

void
emu_chunked_array_deinit(emu_chunked_array * self)
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
emu_chunked_array_free(emu_chunked_array * self)
{
    emu_chunked_array_deinit(self);
    free(self);
}


void *
emu_chunked_array_index(emu_chunked_array * self, size_t i)
{
    assert(self->data);

    // Array bounds check
    size_t elements_per_chunk = 1 << self->log2_elements_per_chunk;
    assert(i < self->num_chunks * elements_per_chunk);

    // First we determine which chunk holds element i
    // i / N
    size_t chunk = i >> self->log2_elements_per_chunk;

    // Then calculate the position of element i within the chunk
    // i % N
    size_t mask = elements_per_chunk - 1;
    size_t offset = i & mask;

    // return data[i / N][i % N]
    void * base = self->data[chunk];
    //void * base = mw_arrayindex((long*)self->data, chunk, self->num_chunks, elements_per_chunk * self->element_size);
    void * ptr = base + (offset * self->element_size);
    return ptr;
}


size_t
emu_chunked_array_size(emu_chunked_array * self)
{
    assert(self->data);
    return self->num_elements;
}
