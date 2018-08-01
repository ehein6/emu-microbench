#pragma once

#include <stdarg.h>

/**
 * Implements a distributed parallel reduce over a @c malloc1dlong() array.
 * @param array Pointer to striped array allocated with @c malloc1dlong().
 * @param size Length of the array (number of elements)
 * @param grain Minimum number of elements to assign to each thread.
 * @param worker worker function that will be called on each array slice in parallel.
 * The loop within the worker function should go from @c begin to @c end and have a
 * stride of @c NODELETS(). Each worker function will be assigned elements on a
 * single nodelet.
 * @param ... Additional arguments to pass to each invocation of the
 * worker function. Arguments will be passed via the varargs interface, and you will need to cast
 * back to the appropriate type within the worker function using the @c va_arg macro.
 * @return The sum of all the elements in the array
 */
long
emu_1d_array_reduce_sum(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    ...
);

/**
 * Like emu_1d_array_reduce_sum, but accepts a va_list to allow forwarding of varargs.
 */
long
emu_1d_array_reduce_sum_var(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    va_list args
);