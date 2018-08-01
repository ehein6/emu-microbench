#include "emu_reduce_1d.h"

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#include <cilk/cilk.h>

static void
emu_1d_array_reduce_sum_level1(
    void * hint,
    long * array,
    long size,
    long grain,
    long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, va_list args),
    va_list args)
{
    (void)hint;
    long local_sum = 0;
    long stride = grain*NODELETS();

    // We will need a copy of the va_list for each loop iteration
    long num_spawns = ((stride - 1) + size) / stride;
    va_list args_copy[num_spawns];
    long args_copy_id = 0;

    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    for (long i = NODE_ID(); i < size; i += stride) {
        va_copy(args_copy[args_copy_id], args);
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, args_copy[args_copy_id++]);
    }
    // Wait for local sum to be computed
    cilk_sync;
    // Add to global sum
    REMOTE_ADD(sum, local_sum);
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

long
emu_1d_array_reduce_sum_var(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    va_list args)
{
    long sum = 0;
    // We will need a copy of the va_list for each nodelet
    va_list args_copy[NODELETS()];

    // Spawn a thread on each nodelet
    const long nodelets = NODELETS();
    for (long i = 0; i < nodelets && i < size; ++i) {
        va_copy(args_copy[i], args);
        cilk_spawn emu_1d_array_reduce_sum_level1(&array[i], array, size, grain, &sum,
            worker, args_copy[i]
        );
    }
    // Wait for thread to complete
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < nodelets; ++i) {
        va_end(args_copy[i]);
    }
    return sum;
}

long
emu_1d_array_reduce_sum(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    ...)
{
    va_list args;
    va_start(args, worker);
    long sum = emu_1d_array_reduce_sum_var(array, size, grain, worker, args);
    va_end(args);
    return sum;
}
