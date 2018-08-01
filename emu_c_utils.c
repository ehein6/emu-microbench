/*! \file emu_c_utils.h
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu c utilities
 */
#include "emu_c_utils.h"
#include <assert.h>
#include <string.h>
#include <cilk/cilk.h>

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

void
replicate_struct(void * s, size_t sz)
{
    // Copy struct to replicated storage on each nodelet
    for (long i = 1; i < NODELETS(); ++i) {
        memcpy(mw_get_nth(s, i), mw_get_nth(s, 0), sz);
    }
}

void
emu_for_each_nodelet_var(
    long * array,
    void (*worker)(void * hint, long node_id, va_list args),
    va_list args
) {
    const long nodelets = NODELETS();
    va_list args_copy[nodelets];
    for (long i = 0; i < nodelets; ++i) {
        void * local = mw_get_nth(array, i);
        va_copy(args_copy[i], args);
        cilk_spawn worker(local, i, args_copy[i]);
    }
    cilk_sync;
    // Clean up va_lists
    for (long i = 0; i < nodelets; ++i) {
        va_end(args_copy[i]);
    }
}

void emu_for_each_nodelet(
    long * array,
    void (*worker)(void * hint, long nodelet_id, va_list args),
    ...
) {
    va_list args;
    va_start(args, worker);
    emu_for_each_nodelet_var(array, worker, args);
    va_end(args);
}
