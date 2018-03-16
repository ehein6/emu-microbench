/*! \file emu_c_utils.h
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu c utilities
 */
#include "emu_c_utils.h"
#include <assert.h>
#include <string.h>

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
