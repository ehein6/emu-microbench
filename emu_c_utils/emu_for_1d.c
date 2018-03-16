#include "emu_for_1d.h"

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
        emu_1d_array_apply_v${num_args}_level1(void * hint, long * array, long size, long grain,
            void (*worker)(long * array, long begin, long end${arg_decls})
            ${arg_decls})
        {
            (void)hint;
            // Spawn threads to handle all the elements on this nodelet
            // Start with an offset of NODE_ID() and a stride of NODELETS()
            long stride = grain*NODELETS();
            for (long i = NODE_ID(); i < size; i += stride) {
                long first = i;
                long last = first + stride; if (last > size) { last = size; }
                cilk_spawn worker(array, first, last${arg_list});
            }
        }

        void
        emu_1d_array_apply_v${num_args}(long * array, long size, long grain,
            void (*worker)(long * array, long begin, long end${arg_decls})
            ${arg_decls})
        {
            // Spawn a thread on each nodelet
            for (long i = 0; i < NODELETS() && i < size; ++i) {
                cilk_spawn emu_1d_array_apply_v${num_args}_level1(&array[i], array, size, grain,
                    worker${arg_list}
                );
            }
        }

    """)
    cog.out(functions.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
static noinline void
emu_1d_array_apply_v0_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end)
    )
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last);
    }
}

void
emu_1d_array_apply_v0(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end)
    )
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v0_level1(&array[i], array, size, grain,
            worker
        );
    }
}

static noinline void
emu_1d_array_apply_v1_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1)
    , void * arg1)
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, arg1);
    }
}

void
emu_1d_array_apply_v1(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1)
    , void * arg1)
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v1_level1(&array[i], array, size, grain,
            worker, arg1
        );
    }
}

static noinline void
emu_1d_array_apply_v2_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, arg1, arg2);
    }
}

void
emu_1d_array_apply_v2(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2)
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v2_level1(&array[i], array, size, grain,
            worker, arg1, arg2
        );
    }
}

static noinline void
emu_1d_array_apply_v3_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3);
    }
}

void
emu_1d_array_apply_v3(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3)
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v3_level1(&array[i], array, size, grain,
            worker, arg1, arg2, arg3
        );
    }
}

static noinline void
emu_1d_array_apply_v4_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3, arg4);
    }
}

void
emu_1d_array_apply_v4(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4)
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v4_level1(&array[i], array, size, grain,
            worker, arg1, arg2, arg3, arg4
        );
    }
}

static noinline void
emu_1d_array_apply_v5_level1(void * hint, long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    (void)hint;
    // Spawn threads to handle all the elements on this nodelet
    // Start with an offset of NODE_ID() and a stride of NODELETS()
    long stride = grain*NODELETS();
    for (long i = NODE_ID(); i < size; i += stride) {
        long first = i;
        long last = first + stride; if (last > size) { last = size; }
        cilk_spawn worker(array, first, last, arg1, arg2, arg3, arg4, arg5);
    }
}

void
emu_1d_array_apply_v5(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
{
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS() && i < size; ++i) {
        cilk_spawn emu_1d_array_apply_v5_level1(&array[i], array, size, grain,
            worker, arg1, arg2, arg3, arg4, arg5
        );
    }
}

/* [[[end]]] */
