/*! \file emu_for_2D
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu 2D chunked array objects and access
 */
#include "emu_chunked_array.h"
#include <cilk/cilk.h>

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

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
        cilk_spawn worker(array, first, last, args_copy[args_copy_id++]);
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

void
emu_chunked_array_apply(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    ...
) {
    va_list args;
    va_start(args, worker);
    emu_chunked_array_apply_var(array, grain, worker, args);
    va_end(args);
}

void
emu_chunked_array_apply_var(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, va_list args),
    va_list args
)
{
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
}

static noinline void
emu_chunked_array_set_long_worker(emu_chunked_array * array, long begin, long end, va_list args)
{
    long value = va_arg(args, long);
    long *p = emu_chunked_array_index(array, begin);
    for (long i = 0; i < end-begin; ++i) {
        p[i] = value;
    }
}

void
emu_chunked_array_set_long(emu_chunked_array * array, long value)
{
    emu_chunked_array_apply(array, GLOBAL_GRAIN(emu_chunked_array_size(array)),
        emu_chunked_array_set_long_worker, (void*)value
    );
}
