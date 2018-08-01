/*! \file emu_for_local
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu local for
 */
#include <cilk/cilk.h>
#include <stdarg.h>
#include "emu_grain_helpers.h"

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

void
emu_local_for_var(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    va_list args)
{
    // We will need a copy of the va_list for each loop iteration
    long num_spawns = ((grain - 1) + end-begin) / grain;
    va_list args_copy[num_spawns];
    long args_copy_id = 0;

    for (long i = begin; i < end; i += grain) {
        va_copy(args_copy[args_copy_id], args);
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, args_copy[args_copy_id++]);
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < num_spawns; ++i) {
        va_end(args_copy[i]);
    }
}

void
emu_local_for(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    ...)
{
    va_list args;
    va_start(args, worker);
    emu_local_for_var(begin, end, grain, worker, args);
    va_end(args);
}

static noinline void
emu_local_for_set_long_worker(long begin, long end, va_list args)
{
    long * array = va_arg(args, long*);
    long value = va_arg(args, long);
    for (long i = begin; i < end; ++i) {
        array[i] = value;
    }
}

void
emu_local_for_set_long(long * array, long n, long value)
{
    emu_local_for(0, n, LOCAL_GRAIN(n),
        emu_local_for_set_long_worker, array, value
    );
}

static noinline void
emu_local_for_copy_long_worker(long begin, long end, va_list args)
{
    long * dst = va_arg(args, long*);
    long * src = va_arg(args, long*);
    for (long i = begin; i < end; ++i) {
        dst[i] = src[i];
    }
}

void
emu_local_for_copy_long(long * dst, long * src, long n)
{
    emu_local_for(0, n, LOCAL_GRAIN_MIN(n, 64),
        emu_local_for_copy_long_worker, dst, src
    );
}