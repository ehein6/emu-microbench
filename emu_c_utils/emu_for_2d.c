#include "emu_chunked_array.h"
#include <cilk/cilk.h>

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif


/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    arg_list = "".join([", arg%i"%(i+1) for i in xrange(num_args)])

    functions=Template("""
        static noinline void
        emu_chunked_array_apply_v${num_args}_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
            void (*worker)(emu_chunked_array * array, long begin, long end${arg_decls})
            ${arg_decls})
        {
            (void)hint;
            for (long i = begin; i < end; i += grain) {
                long first = i;
                long last = first + grain; if (last > end) { last = end; }
                cilk_spawn worker(array, first, last${arg_list});
            }
        }

        void
        emu_chunked_array_apply_v${num_args}(emu_chunked_array * array, long grain,
            void (*worker)(emu_chunked_array * array, long begin, long end${arg_decls})
            ${arg_decls})
        {
            long n = emu_chunked_array_size(array);
            // Each thread will be responsible for the elements on one nodelet
            long local_n = n / NODELETS();
            // Spawn a thread on each nodelet
            for (long i = 0; i < NODELETS(); ++i) {
                long begin = local_n * i;
                long end = local_n * (i + 1); if (end > n) { end = n; }
                cilk_spawn emu_chunked_array_apply_v${num_args}_level1(array->data[i], array, begin, end, grain,
                    worker${arg_list}
                );
            }
        }

    """)
    cog.out(functions.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
static noinline void
emu_chunked_array_apply_v0_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end)
    )
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last);
    }
}

void
emu_chunked_array_apply_v0(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end)
    )
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v0_level1(array->data[i], array, begin, end, grain,
            worker
        );
    }
}

static noinline void
emu_chunked_array_apply_v1_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1)
    , void * arg1)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, arg1);
    }
}

void
emu_chunked_array_apply_v1(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1)
    , void * arg1)
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v1_level1(array->data[i], array, begin, end, grain,
            worker, arg1
        );
    }
}

static noinline void
emu_chunked_array_apply_v2_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, arg1, arg2);
    }
}

void
emu_chunked_array_apply_v2(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v2_level1(array->data[i], array, begin, end, grain,
            worker, arg1, arg2
        );
    }
}

static noinline void
emu_chunked_array_apply_v3_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3);
    }
}

void
emu_chunked_array_apply_v3(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v3_level1(array->data[i], array, begin, end, grain,
            worker, arg1, arg2, arg3
        );
    }
}

static noinline void
emu_chunked_array_apply_v4_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3, arg4);
    }
}

void
emu_chunked_array_apply_v4(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v4_level1(array->data[i], array, begin, end, grain,
            worker, arg1, arg2, arg3, arg4
        );
    }
}

static noinline void
emu_chunked_array_apply_v5_level1(void * hint, emu_chunked_array * array, long begin, long end, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3, arg4, arg5);
    }
}

void
emu_chunked_array_apply_v5(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    long n = emu_chunked_array_size(array);
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v5_level1(array->data[i], array, begin, end, grain,
            worker, arg1, arg2, arg3, arg4, arg5
        );
    }
}

/* [[[end]]] */

static noinline void
emu_chunked_array_set_long_worker(emu_chunked_array * array, long begin, long end, void * arg1)
{
    long value = (long) arg1;
    long *p = emu_chunked_array_index(array, begin);
    for (long i = 0; i < end-begin; ++i) {
        p[i] = value;
    }
}

void
emu_chunked_array_set_long(emu_chunked_array * array, long value)
{
    emu_chunked_array_apply_v1(array, GLOBAL_GRAIN(emu_chunked_array_size(array)),
        emu_chunked_array_set_long_worker, (void*)value
    );
}