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

// Zero-argument version

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

// Assumes array is an array allocated with mw_malloc2d(NODELETS(), element_size * n/NODELETS())
void
emu_chunked_array_apply_v0(void ** array, long n, long grain,
    void (*worker)(long begin, long end))
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

// One-argument version

static noinline void
emu_chunked_array_apply_v1_level1(void * hint, long begin, long end, long grain,
    void * arg1,
    void (*worker)(long begin, long end, void * arg1)
)
{
    (void)hint;
    for (long i = begin; i < end; i += grain) {
        long first = i;
        long last = first + grain; if (last > end) { last = end; }
        cilk_spawn worker(first, last, arg1);
    }
}

// Assumes array is an array allocated with mw_malloc2d(NODELETS(), element_size * n/NODELETS())
void
emu_chunked_array_apply_v1(void ** array, long n, long grain,
    void * arg1,
    void (*worker)(long begin, long end, void * arg1))
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