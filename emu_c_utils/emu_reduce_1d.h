#pragma once

#include <stdarg.h>

long
emu_1d_array_reduce_sum(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    ...
);

long
emu_1d_array_reduce_sum_var(
    long * array,
    long size,
    long grain,
    void (*worker)(long * array, long begin, long end, long * sum, va_list args),
    va_list args
);