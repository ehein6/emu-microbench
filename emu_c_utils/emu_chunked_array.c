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

static noinline void
emu_chunked_array_apply_var_level1(
    void * hint,
    emu_chunked_array * array,
    long begin,
    long end,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    va_list args
)
{
    (void)hint;

    // We will need a copy of the va_list for each loop iteration
    long num_spawns = ((grain - 1) + end-begin) / grain;
    va_list args_copy[num_spawns];
    long args_copy_id = 0;

    for (long i = begin; i < end; i += grain) {
        va_copy(args_copy[args_copy_id], args);
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, args_copy[args_copy_id]);
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

void
emu_chunked_array_apply_var(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    ...
)
{
    va_list args;
    va_start(args, worker);

    // We will need a copy of the va_list for each nodelet
    va_list args_copy[NODELETS()];

    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        va_copy(args_copy[i], args);
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_var_level1(array->data[i], array, begin, end, grain,
            worker, args_copy[i]
        );
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < NODELETS(); ++i) {
        va_end(args_copy[i]);
    }
    va_end(args);
}


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
    REPLICATE(self_0);
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
