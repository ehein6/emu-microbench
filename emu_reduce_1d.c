#include "emu_reduce_1d.h"

#if defined(__le64__)
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

#include <cilk/cilk.h>

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    arg_list = "".join([", arg%i"%(i+1) for i in xrange(num_args)])

    functions=Template("""
        static noinline void
        emu_1d_array_reduce_sum_v${num_args}_level1(void * hint, long * array, long size, long grain, long * sum,
            void (*worker)(long * array, long begin, long end, long *sum${arg_decls})
            ${arg_decls})
        {
            (void)hint;
            long local_sum = 0;
            // Spawn threads to handle all the elements on this nodelet
            // Start with an offset of NODE_ID() and a stride of NODELETS()
            long stride = grain*NODELETS();
            for (long i = NODE_ID(); i < size; i += stride) {
                long first = i;
                long last = first + stride; if (last > size) { last = size; }
                cilk_spawn worker(array, first, last, &local_sum${arg_list});
            }
            cilk_sync;
            REMOTE_ADD(sum, local_sum);
        }

        long
        emu_1d_array_reduce_sum_v${num_args}(long * array, long size, long grain,
            void (*worker)(long * array, long begin, long end, long * sum${arg_decls})
            ${arg_decls})
        {
            long sum = 0;
            // Spawn a thread on each nodelet
            for (long i = 0; i < NODELETS() && i < size; ++i) {
                cilk_spawn emu_1d_array_reduce_sum_v${num_args}_level1(&array[i], array, size, grain, &sum,
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
emu_1d_array_reduce_sum_v0_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum)
    )
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v0(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum)
    )
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v0_level1(&array[i], array, size, grain, &sum,
            worker
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_1d_array_reduce_sum_v1_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, void * arg1)
    , void * arg1)
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, arg1);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v1(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1)
    , void * arg1)
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v1_level1(&array[i], array, size, grain, &sum,
            worker, arg1
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_1d_array_reduce_sum_v2_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v2(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v2_level1(&array[i], array, size, grain, &sum,
            worker, arg1, arg2
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_1d_array_reduce_sum_v3_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v3(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v3_level1(&array[i], array, size, grain, &sum,
            worker, arg1, arg2, arg3
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_1d_array_reduce_sum_v4_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3, arg4);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v4(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v4_level1(&array[i], array, size, grain, &sum,
            worker, arg1, arg2, arg3, arg4
        );
    }
    cilk_sync;
    return sum;
}

static noinline void
emu_1d_array_reduce_sum_v5_level1(void * hint, long * array, long size, long grain, long * sum,
    void (*worker)(long * array, long begin, long end, long *sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    (void)hint;
    long local_sum = 0;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, &local_sum, arg1, arg2, arg3, arg4, arg5);
    }
    cilk_sync;
    REMOTE_ADD(sum, local_sum);
}

long
emu_1d_array_reduce_sum_v5(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    long sum = 0;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_reduce_sum_v5_level1(&array[i], array, size, grain, &sum,
            worker, arg1, arg2, arg3, arg4, arg5
        );
    }
    cilk_sync;
    return sum;
}

/* [[[end]]] */