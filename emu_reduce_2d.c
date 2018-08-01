/*! \file emu_reduce_2d
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu 2D reductions
 */
#include "emu_chunked_array.h"
#include <cilk/cilk.h>

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif


static void
emu_chunked_array_reduce_sum_level1(
    void * hint,
    emu_chunked_array * array,
    long begin,
    long end,
    long grain,
    long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, va_list args),
    va_list args)
{
    (void)hint;
    long local_sum = 0;

    // We will need a copy of the va_list for each loop iteration
    long num_spawns = ((grain - 1) + end-begin) / grain;
    va_list args_copy[num_spawns];
    long args_copy_id = 0;

    for (long i = begin; i < end; i += grain) {
        va_copy(args_copy[args_copy_id], args);
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, args_copy[args_copy_id++]);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

long
emu_chunked_array_reduce_sum_var(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, va_list args),
    va_list args)
{
    // We will need a copy of the va_list for each nodelet
    const long nodelets = NODELETS();
    va_list args_copy[nodelets];

    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < nodelets; ++i) {
        va_copy(args_copy[i], args);
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_sum_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, args_copy[i]
        );
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < nodelets; ++i) {
        va_end(args_copy[i]);
    }
    return sum;
}

long
emu_chunked_array_reduce_sum(
    emu_chunked_array * array,
    long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, va_list args),
    ...)
{
    va_list args;
    va_start(args, worker);
    long sum = emu_chunked_array_reduce_sum_var(array, grain, worker, args);
    va_end(args);
    return sum;
}

static void
emu_chunked_array_reduce_long_worker(emu_chunked_array * array, long begin, long end, long * sum, va_list args)
{
    long partial_sum = 0;
    long *p = emu_chunked_array_index(array, begin);
    for (long i = 0; i < end-begin; ++i) {
        partial_sum += p[i];
    }
    REMOTE_ADD(sum, partial_sum);
}

long
emu_chunked_array_reduce_sum_long(emu_chunked_array * array)
{
    return emu_chunked_array_reduce_sum(array, GLOBAL_GRAIN(emu_chunked_array_size(array)),
        emu_chunked_array_reduce_long_worker
    );
}
