#include <cilk/cilk.h>
#include "emu_grain_helpers.h"

#ifdef __le64__
#include <memoryweb.h>
#else
// Mimic memoryweb behavior on x86
// TODO eventually move this all to its own header file
#define NODELETS() (1)
#define noinline __attribute__ ((noinline))
#endif


/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    arg_list = "".join([", arg%i"%(i+1) for i in xrange(num_args)])

    function=Template("""
        void
        emu_local_for_v${num_args}(long begin, long end, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls})
        {
            for (long i = begin; i < end; i += grain) {
                long first = i;
                long last = first + grain <= end ? first + grain : end;
                cilk_spawn worker(first, last${arg_list});
            }
        }

    """)
    cog.out(function.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
void
emu_local_for_v0(long begin, long end, long grain,
    void (*worker)(long begin, long end)
    )
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last);
    }
}

void
emu_local_for_v1(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, arg1);
    }
}

void
emu_local_for_v2(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, arg1, arg2);
    }
}

void
emu_local_for_v3(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, arg1, arg2, arg3);
    }
}

void
emu_local_for_v4(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, arg1, arg2, arg3, arg4);
    }
}

void
emu_local_for_v5(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain <= end ? first + grain : end;
        cilk_spawn worker(first, last, arg1, arg2, arg3, arg4, arg5);
    }
}

/* [[[end]]] */

static noinline void
emu_local_for_set_long_worker(long begin, long end, void * arg1, void * arg2)
{
    long * array = arg1;
    long value = (long)arg2;
    for (long i = begin; i < end; ++i) {
        array[i] = value;
    }
}

void
emu_local_for_set_long(long * array, long n, long value)
{
    emu_local_for_v2(0, n, LOCAL_GRAIN(n),
        emu_local_for_set_long_worker, array, (void*)value
    );
}