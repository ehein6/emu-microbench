/*! \file emu_grain_helpers
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu grain size helpers
 */
#pragma once

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#define THREADS_PER_GC (64L)

long get_gcs_per_nodelet();

static inline long
GLOBAL_GRAIN(long n)
{
    long global_num_threads = THREADS_PER_GC * get_gcs_per_nodelet() * NODELETS();
    return n > global_num_threads ? (n/global_num_threads) : 1;
}

static inline long
LOCAL_GRAIN(long n)
{
    long local_num_threads = THREADS_PER_GC * get_gcs_per_nodelet();
    return n > local_num_threads ? (n/local_num_threads) : 1;
}