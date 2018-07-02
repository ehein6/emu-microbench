#include "emu_for_1d.h"

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#include <cilk/cilk.h>

static noinline void
emu_1d_array_apply_var_level1(
    void * hint,
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, va_list args),
    va_list args)
{
    (void)hint;

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
        cilk_spawn worker(array, first, last, args_copy[args_copy_id++]);
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

void
emu_1d_array_apply(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, va_list args),
    ...
) {
    va_list args;
    va_start(args, worker);
    emu_1d_array_apply_var(array, size, grain, worker, args);
    va_end(args);
}

void
emu_1d_array_apply_var(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, va_list args),
    va_list args
) {
    // We will need a copy of the va_list for each nodelet
    va_list args_copy[NODELETS()];

    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        va_copy(args_copy[i], args);
        cilk_spawn emu_1d_array_apply_var_level1(&array[i], array, size, grain,
            worker, args_copy[i]
        );
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < NODELETS(); ++i) {
        va_end(args_copy[i]);
    }
}
