#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include "timer.h"
#include "recursive_spawn.h"

#ifdef __le64__
#include <memoryweb.h>
#else

#define NODELETS() (8)
#define replicated
#define PRIORITY(X) (63-__builtin_clzl(X))

void *
mw_malloc2d(size_t nelem, size_t sz)
{
    // We need an 8-byte pointer for each element, plus the array of elements
    size_t bytes = nelem * sizeof(long) + nelem * sz;
    unsigned char ** ptrs = malloc(bytes);
    // Skip past the pointers to get to the raw array
    unsigned char * data = (unsigned char *)ptrs + nelem * sizeof(long);
    // Assign pointer to each element
    for (size_t i = 0; i < nelem; ++i) {
        ptrs[i] = data + i * sz;
    }
    return ptrs;
}

void *
mw_malloc1dlong(size_t nelem)
{
    return malloc(nelem * sizeof(long));
}

void
mw_free(void * ptr)
{
    free(ptr);
}

#endif

typedef struct global_stream_data {
    long ** a;
    long ** b;
    long ** c;
    long n;
    long num_threads;
} global_stream_data;


// #define INDEX(PTR, BLOCK, I) (PTR[I/BLOCK][I%BLOCK])
#define INDEX(PTR, BLOCK, I) (PTR[I >> PRIORITY(BLOCK)][I&(BLOCK-1)])

void
global_stream_init(global_stream_data * data, long n)
{
    data->n = n;
    long block_sz = sizeof(long) * n/NODELETS();
    data->a = mw_malloc2d(NODELETS(), block_sz);
    assert(data->a);
    data->b = mw_malloc2d(NODELETS(), block_sz);
    assert(data->b);
    data->c = mw_malloc2d(NODELETS(), block_sz);
    assert(data->c);

    for (long i = 0; i < NODELETS(); ++i) {
        cilk_spawn memset(data->a[i], 0, block_sz);
        cilk_spawn memset(data->b[i], 0, block_sz);
        cilk_spawn memset(data->c[i], 0, block_sz);
    }
    cilk_sync;

#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        global_stream_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(global_stream_data));
    }
#endif
}

void
global_stream_deinit(global_stream_data * data)
{
    mw_free(data->a);
    mw_free(data->b);
    mw_free(data->c);
}

void
global_stream_add_serial(global_stream_data * data)
{
    long block_sz = sizeof(long) * data->n / NODELETS();
    for (long i = 0; i < data->n; ++i) {
        INDEX(data->c, block_sz, i) = INDEX(data->a, block_sz, i) + INDEX(data->b, block_sz, i);
    }
}

void
global_stream_add_cilk_for(global_stream_data * data)
{
    long block_sz = sizeof(long) * data->n / NODELETS();
    #pragma cilk grainsize = data->n / data->num_threads
    cilk_for (long i = 0; i < data->n; ++i) {
        INDEX(data->c, block_sz, i) = INDEX(data->a, block_sz, i) + INDEX(data->b, block_sz, i);
    }
}

static void
recursive_spawn_add_worker(long begin, long end, global_stream_data *data)
{
    long block_sz = sizeof(long) * data->n / NODELETS();
    for (long i = begin; i < end; ++i) {
        INDEX(data->c, block_sz, i) = INDEX(data->a, block_sz, i) + INDEX(data->b, block_sz, i);
    }
}

static void
recursive_spawn_add(long begin, long end, long grain, global_stream_data *data)
{
    RECURSIVE_CILK_SPAWN(begin, end, grain, recursive_spawn_add, data);
}

void
global_stream_add_recursive_spawn(global_stream_data * data)
{
    recursive_spawn_add(0, data->n, data->n / data->num_threads, data);
}

void
global_stream_add_serial_spawn(global_stream_data * data)
{
    long grain = data->n / data->num_threads;
    for (long i = 0; i < data->n; i += grain) {
        long begin = i;
        long end = begin + grain <= data->n ? begin + grain : end;
        cilk_spawn recursive_spawn_add_worker(begin, end, data);
    }
    cilk_sync;
}

#define RUN_BENCHMARK(X) \
do {                                                        \
    timer_start();                                          \
    X (&data);                                              \
    long ticks = timer_stop();                              \
    double bw = timer_calc_bandwidth(ticks, data.n * sizeof(long) * 3); \
    timer_print_bandwidth( #X , bw);                        \
} while (0)


replicated global_stream_data data;

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_elements;
        long num_threads;
    } args;

    if (argc != 4) {
        printf("Usage: %s mode num_elements num_threads\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_threads = atol(argv[3]);

        if (args.log2_num_elements <= 0) { printf("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { printf("num_threads must be > 0"); exit(1); }
    }

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    printf("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", n, mbytes, mbytes_per_nodelet);
    fflush(stdout);
    data.num_threads = args.num_threads;
    global_stream_init(&data, n);
    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(global_stream_add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(global_stream_add_serial_spawn);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(global_stream_add_recursive_spawn);
    } else if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(global_stream_add_serial);
    } else {
        printf("Mode %s not implemented!", args.mode);
    }

    global_stream_deinit(&data);
    return 0;
}
