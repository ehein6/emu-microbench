/*! \file emu_c_utils.h
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu c utilities
 */
#pragma once

#include <stddef.h>
#include <stdarg.h>

#define REPLICATE(X) replicate_struct((X), sizeof(*(X)))
void replicate_struct(void * s, size_t sz);

/**
 * Execute a function on each nodelet in parallel
 * @param array Pointer to any replicated variable
 * @param worker Function that will be executed on each nodelet.
 * @param ...
 */
void emu_for_each_nodelet(
    long * array,
    void (*worker)(void * hint, long nodelet_id, va_list args),
    ...
);

/**
 * Like emu_for_each_nodelet, but accepts a va_list to allow forwarding of varargs.
 */
void emu_for_each_nodelet_var(
    long * array,
    void (*worker)(void * hint, long nodelet_id, va_list args),
    va_list args
);
