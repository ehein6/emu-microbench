/*! \file emu_for_local
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu local for
 */
#pragma once

#include <stdarg.h>
#include "emu_grain_helpers.h"

/**
 * Applies a function to a range in parallel.
 * These loops can be replaced with @c cilk_for once it is working.
 * @param begin beginning of the iteration space (usually 0)
 * @param end end of the iteration space (usually array length)
 * @param grain Minimum number of elements to assign to each thread
 * @param worker worker function that will be called on each array slice in parallel.
 *  The loop within the worker function should go from @p begin to @p end with a stride
 *  of 1.
 * @param ... Additional arguments to pass to each invocation of the
 *  worker function. Arguments will be passed via the varargs interface, and you will need to cast
 *  back to the appropriate type within the worker function using the @c va_arg macro.
 */
void
emu_local_for(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    ...
);

/**
 * Like emu_local_for, but accepts a va_list to allow forwarding of varargs.
 */
void
emu_local_for_var(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    va_list args
);

/**
 * Sets each value of @c array to @c value in parallel
 * @param array Pointer to array to modify
 * @param n Number of elements in the array
 * @param value Value to set
 */
void
emu_local_for_set_long(long * array, long n, long value);

/**
 * Copies @c src to @c dst in parallel
 * @param dst Pointer to destination array
 * @param src Pointer to source array
 * @param n Array length
 */
void
emu_local_for_copy_long(long * dst, long * src, long n);
