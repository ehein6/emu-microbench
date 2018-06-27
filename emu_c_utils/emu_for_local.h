/*! \file emu_for_local
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu local for
 */
#pragma once

#include <stdarg.h>
#include "emu_grain_helpers.h"

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])

    function=Template("""
        void
        emu_local_for_v${num_args}(long begin, long end, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls});

    """)
    cog.out(function.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
void
emu_local_for_v0(long begin, long end, long grain,
    void (*worker)(long begin, long end)
    );

void
emu_local_for_v1(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1);

void
emu_local_for_v2(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2);

void
emu_local_for_v3(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3);

void
emu_local_for_v4(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4);

void
emu_local_for_v5(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5);

/* [[[end]]] */

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
