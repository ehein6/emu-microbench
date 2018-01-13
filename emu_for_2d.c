#include "emu_for_2d.h"
#include <cilk/cilk.h>

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

    functions=Template("""
        static noinline void
        emu_chunked_array_apply_v${num_args}_level1(void * hint, long begin, long end, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls})
        {
            (void)hint;
            for (long i = begin; i < end; i += grain) {
                long first = i;
                long last = first + grain; if (last > end) { last = end; }
                cilk_spawn worker(first, last${arg_list});
            }
        }

        void
        emu_chunked_array_apply_v${num_args}(void ** array, long n, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls})
        {
            // Each thread will be responsible for the elements on one nodelet
            long local_n = n / NODELETS();
            // Spawn a thread on each nodelet
            for (long i = 0; i < NODELETS(); ++i) {
                long begin = local_n * i;
                long end = local_n * (i + 1); if (end > n) { end = n; }
                cilk_spawn emu_chunked_array_apply_v${num_args}_level1(array[i], begin, end, grain${arg_list}, worker);
            }
        }

    """)
    cog.out(functions.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
static noinline void
emu_chunked_array_apply_v0_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end)
    )
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last);
    }
}

void
emu_chunked_array_apply_v0(void ** array, long n, long grain,
    void (*worker)(long begin, long end)
    )
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v0_level1(array[i], begin, end, grain, worker);
    }
}

static noinline void
emu_chunked_array_apply_v1_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1);
    }
}

void
emu_chunked_array_apply_v1(void ** array, long n, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v1_level1(array[i], begin, end, grain, arg1, worker);
    }
}

static noinline void
emu_chunked_array_apply_v2_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1, arg2);
    }
}

void
emu_chunked_array_apply_v2(void ** array, long n, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v2_level1(array[i], begin, end, grain, arg1, arg2, worker);
    }
}

static noinline void
emu_chunked_array_apply_v3_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1, arg2, arg3);
    }
}

void
emu_chunked_array_apply_v3(void ** array, long n, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v3_level1(array[i], begin, end, grain, arg1, arg2, arg3, worker);
    }
}

static noinline void
emu_chunked_array_apply_v4_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1, arg2, arg3, arg4);
    }
}

void
emu_chunked_array_apply_v4(void ** array, long n, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v4_level1(array[i], begin, end, grain, arg1, arg2, arg3, arg4, worker);
    }
}

static noinline void
emu_chunked_array_apply_v5_level1(void * hint, long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1, arg2, arg3, arg4, arg5);
    }
}

void
emu_chunked_array_apply_v5(void ** array, long n, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = n / NODELETS();
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_apply_v5_level1(array[i], begin, end, grain, arg1, arg2, arg3, arg4, arg5, worker);
    }
}

/* [[[end]]] */