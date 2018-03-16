/*! \file emu_reduce_2d
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu 2D reductions
 */
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
        emu_chunked_array_reduce_v${num_args}_level1(void * hint, emu_chunked_array * array,
            long begin, long end, long grain, long * sum,
            void (*worker)(emu_chunked_array * array, long begin, long end, long * sum${arg_decls})
            ${arg_decls})
        {
            (void)hint;
            long local_sum = 0;
            for (long i = begin; i < end; i += grain) {
                long first = i;
                long last = first + grain; if (last > end) { last = end; }
                cilk_spawn worker(array, first, last, &local_sum${arg_list});
            }
            cilk_sync;
            REMOTE_ADD(sum, local_sum);
        }

        long
        emu_chunked_array_reduce_sum_v${num_args}(emu_chunked_array * array, long grain,
            void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum${arg_decls})
            ${arg_decls})
        {
            // Each thread will be responsible for the elements on one nodelet
            long n = array->num_elements;
            long local_n = 1 << array->log2_elements_per_chunk;
            long sum = 0;
            // Spawn a thread on each nodelet
            for (long i = 0; i < NODELETS(); ++i) {
                long begin = local_n * i;
                long end = local_n * (i + 1); if (end > n) { end = n; }
                cilk_spawn emu_chunked_array_reduce_v${num_args}_level1(array->data[i], array,
                    begin, end, grain, &sum,
                    worker${arg_list}
                );
            }
            cilk_sync;
            return sum;
        }

    """)
    cog.out(functions.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
static noinline void
emu_chunked_array_reduce_v0_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum)
    )
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v0(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum)
    )
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v0_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_chunked_array_reduce_v1_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1)
    , void * arg1)
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, arg1);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v1(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, void * arg1)
    , void * arg1)
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v1_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, arg1
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_chunked_array_reduce_v2_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v2(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v2_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, arg1, arg2
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_chunked_array_reduce_v3_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v3(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v3_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, arg1, arg2, arg3
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_chunked_array_reduce_v4_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3, arg4);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v4(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v4_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, arg1, arg2, arg3, arg4
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_chunked_array_reduce_v5_level1(void * hint, emu_chunked_array * array,
    long begin, long end, long grain, long * sum,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    (void)hint;
    long local_sum = 0;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3, arg4, arg5);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_chunked_array_reduce_sum_v5(emu_chunked_array * array, long grain,
    void (*worker)(emu_chunked_array * array, long begin, long end, long * partial_sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    // Each thread will be responsible for the elements on one nodelet
    long n = array->num_elements;
    long local_n = 1 << array->log2_elements_per_chunk;
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        long begin = local_n * i;
        long end = local_n * (i + 1); if (end > n) { end = n; }
        cilk_spawn emu_chunked_array_reduce_v5_level1(array->data[i], array,
            begin, end, grain, &sum,
            worker, arg1, arg2, arg3, arg4, arg5
        );
    }
    cilk_sync;
    return sum;
}

/* [[[end]]] */

static noinline void
emu_chunked_array_reduce_long_worker(emu_chunked_array * array, long begin, long end, long * sum)
{
    long partial_sum = 0;
    long *p = emu_chunked_array_index(array, begin);
    for (long i = 0; i < end-begin; ++i) {
        partial_sum += p[i];
    }
    REMOTE_ADD(sum, partial_sum);
}

long
emu_chunked_array_reduce_sum_long(emu_chunked_array * array)
{
    return emu_chunked_array_reduce_sum_v0(array, GLOBAL_GRAIN(emu_chunked_array_size(array)),
        emu_chunked_array_reduce_long_worker
    );
}
