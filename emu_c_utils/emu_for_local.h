/*! \file emu_for_local
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu local for
 */
#pragma once

#include <stdarg.h>
#include "emu_grain_helpers.h"

void
emu_local_for(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    ...
);

void
emu_local_for_var(
    long begin,
    long end,
    long grain,
    void (*worker)(long begin, long end, va_list args),
    va_list args
);

/**
 * Set each value of @c array to @c value in parallel
 * @param array Pointer to array to modify
 * @param n Number of elements in the array
 * @param value Value to set
 */
void
emu_local_for_set_long(long * array, long n, long value);

void
emu_local_for_copy_long(long * dst, long * src, long n);
