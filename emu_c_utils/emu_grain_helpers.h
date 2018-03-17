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

/**
 * Calculates the grain size for a loop of @c n iterations that will spawn the maximum number of threads across the
 * entire system.
 * @param n number of loop iterations
 * @return grain size
 */
static inline long
GLOBAL_GRAIN(long n)
{
    long global_num_threads = THREADS_PER_GC * get_gcs_per_nodelet() * NODELETS();
    return n > global_num_threads ? (n/global_num_threads) : 1;
}

/**
 * Calculates the grain size for a loop of @c n iterations that will spawn the maximum number of threads on one nodelet
 * @param n number of loop iterations
 * @return grain size
 */
static inline long
LOCAL_GRAIN(long n)
{
    long local_num_threads = THREADS_PER_GC * get_gcs_per_nodelet();
    return n > local_num_threads ? (n/local_num_threads) : 1;
}

/**
 * Similar to LOCAL_GRAIN, except the final grain size can never be smaller than @c min_grain
 * @param n number of loop iterations
 * @param min_grain minimum grain size to return
 * @return grain size
 */
static inline long
LOCAL_GRAIN_MIN(long n, long min_grain)
{
    long grain = LOCAL_GRAIN(n);
    return grain > min_grain ? grain : min_grain;
}

/**
 * Similar to GLOBAL_GRAIN, except the final grain size can never be smaller than @c min_grain
 * @param n number of loop iterations
 * @param min_grain minimum grain size to return
 * @return grain size
 */
static inline long
GLOBAL_GRAIN_MIN(long n, long min_grain)
{
    long grain = GLOBAL_GRAIN(n);
    return grain > min_grain ? grain : min_grain;
}
