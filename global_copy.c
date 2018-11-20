#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include "common.h"

#include <emu_c_utils/emu_c_utils.h>
#include "recursive_spawn.h"


typedef struct global_copy_data {
    emu_chunked_array array_a;
    emu_chunked_array array_b;
    long ** a;
    long ** b;
    long n;
    long num_threads;
} global_copy_data;


// #define INDEX(PTR, BLOCK, I) (PTR[I/BLOCK][I%BLOCK])
#define INDEX(PTR, BLOCK, I) (PTR[I >> PRIORITY(BLOCK)][I&(BLOCK-1)])

void
global_copy_init(global_copy_data * data, long n)
{
    data->n = n;
    emu_chunked_array_replicated_init(&data->array_a, n, sizeof(long));
    data->a = (long**)data->array_a.data;
    emu_chunked_array_replicated_init(&data->array_b, n, sizeof(long));
    data->b = (long**)data->array_b.data;

#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        global_copy_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(global_copy_data));
    }
#endif
#ifndef NO_VALIDATE
    emu_chunked_array_set_long(&data->array_a, 1);
    emu_chunked_array_set_long(&data->array_b, 0);
#endif
}

void
global_copy_deinit(global_copy_data * data)
{
    emu_chunked_array_replicated_deinit(&data->array_a);
    emu_chunked_array_replicated_deinit(&data->array_b);
}

static noinline void
global_copy_validate_worker(emu_chunked_array * array, long begin, long end, va_list args)
{
    long * b = emu_chunked_array_index(array, begin);
    for (long i = 0; i < end - begin; ++i) {
        if (b[i] != 1) {
            LOG("VALIDATION ERROR: b[%li] == %li (supposed to be 1)\n", begin + i, b[i]);
            exit(1);
        }
    }
}

void
global_copy_validate(global_copy_data * data)
{
    emu_chunked_array_apply(&data->array_b, GLOBAL_GRAIN(data->n),
        global_copy_validate_worker
    );
}

// serial - just a regular for loop
void
global_copy_serial(global_copy_data * data)
{
    long block_sz = data->n / NODELETS();
    for (long i = 0; i < data->n; ++i) {
        INDEX(data->b, block_sz, i) = INDEX(data->a, block_sz, i);
    }
}

// cilk_for - cilk_for loop with grainsize set to control number of threads
void
global_copy_cilk_for(global_copy_data * data)
{
    long block_sz = data->n / NODELETS();
    #pragma cilk grainsize = data->n / data->num_threads
    cilk_for (long i = 0; i < data->n; ++i) {
        INDEX(data->b, block_sz, i) = INDEX(data->a, block_sz, i);
    }
}

noinline void
recursive_copy_worker(long begin, long end, global_copy_data *data)
{
    long block_sz = data->n / NODELETS();
    for (long i = begin; i < end; ++i) {
        INDEX(data->b, block_sz, i) = INDEX(data->a, block_sz, i);
    }
}

noinline void
recursive_spawn_copy(long begin, long end, long grain, global_copy_data *data)
{
    RECURSIVE_CILK_SPAWN(begin, end, grain, recursive_spawn_copy, data);
}

// recursive_spawn - recursively spawn threads to subdivide the range until the grain size is reached
void
global_copy_recursive_spawn(global_copy_data * data)
{
    recursive_spawn_copy(0, data->n, data->n / data->num_threads, data);
}

// serial_spawn - spawn one thread to handle each grain-sized chunk of the range
void
global_copy_serial_spawn(global_copy_data * data)
{
    long grain = data->n / data->num_threads;
    for (long i = 0; i < data->n; i += grain) {
        long begin = i;
        long end = begin + grain <= data->n ? begin + grain : data->n;
        cilk_spawn recursive_spawn_copyworker(begin, end, data);
    }
    cilk_sync;
}

noinline void
serial_remote_spawn_level2(long begin, long end, long * a, long * b)
{
    for (long i = begin; i < end; ++i) {
        b[i] = a[i];
    }
}

noinline void
serial_remote_spawn_level1(long * a, long * b, long n, long grain)
{
    for (long i = 0; i < n; i += grain) {
        long begin = i;
        long end = begin + grain <= n ? begin + grain : n;
        cilk_spawn serial_remote_spawn_level2(begin, end, a, b);
    }
    cilk_sync;
}

// serial_remote_spawn - remote spawn a thread on each nodelet, then do a serial spawn locally
void
global_copy_serial_remote_spawn(global_copy_data * data)
{
    // Each thread will be responsible for the elements on one nodelet
    long local_n = data->n / NODELETS();
    // Calculate the grain so we get the right number of threads globally
    long grain = data->n / data->num_threads;
    // Spawn a thread on each nodelet
    for (long i = 0; i < NODELETS(); ++i) {
        cilk_spawn_at(data->a[i]) serial_remote_spawn_level1(data->a[i], data->b[i], local_n, grain);
    }
    cilk_sync;
}

noinline void
recursive_remote_spawn_level2_worker(long begin, long end, long * a, long * b)
{
    for (long i = begin; i < end; ++i) {
        b[i] = a[i];
    }
}

noinline void
recursive_remote_spawn_level2(long begin, long end, long grain, long * a, long * b)
{
    RECURSIVE_CILK_SPAWN(begin, end, grain, recursive_remote_spawn_level2, a, b);
}

noinline void
recursive_remote_spawn_level1(long low, long high, global_copy_data * data)
{
    for (;;) {
        long count = high - low;
        if (count == 1) break;
        long mid = low + count / 2;
        cilk_spawn_at(data->a[low]) recursive_remote_spawn_level1(low, mid, data);
        low = mid;
    }

    /* Recursive base case: call worker function */
    long local_n = data->n / NODELETS();
    long grain = data->n / data->num_threads;
    recursive_remote_spawn_level2(0, local_n, grain, data->a[low], data->b[low]);
}

// recursive_remote_spawn - Recursively spawns threads to divice up the loop range, using remote spawns where possible.
void
global_copy_recursive_remote_spawn(global_copy_data * data)
{
    recursive_remote_spawn_level1(0, NODELETS(), data);
}

void
global_copy_library_worker(emu_chunked_array * array, long begin, long end, va_list args)
{
    (void)array;
    global_copy_data * data = va_arg(args, global_copy_data *);
    long block_sz = data->n / NODELETS();

    long * b = &INDEX(data->b, block_sz, begin);
    long * a = &INDEX(data->a, block_sz, begin);

    for (long i = 0; i < end-begin; ++i) {
        b[i] = a[i];
    }
}

void
global_copy_library(global_copy_data * data)
{
    emu_chunked_array_apply(&data->array_a, data->n / data->num_threads,
        global_copy_library_worker, data
    );
}

// serial_remote_spawn_shallow - same as serial_remote_spawn, but with only one level of spawning
void
global_copy_serial_remote_spawn_shallow(global_copy_data * data)
{
    long local_n = data->n / NODELETS();
    long grain = data->n / data->num_threads;

    for (long i = 0; i < NODELETS(); ++i) {
        long * a = data->a[i];
        long * b = data->b[i];
        for (long j = 0; j < local_n; j += grain) {
            long begin = j;
            long end = begin + grain <= local_n ? begin + grain : local_n;
            cilk_spawn_at(a) serial_remote_spawn_level2(begin, end, a, b);
        }
    }
    cilk_sync;
}

void global_copy_run(
    global_copy_data * data,
    const char * name,
    void (*benchmark)(global_copy_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark(data);
        double time_ms = hooks_region_end();
        double bytes_per_second = time_ms == 0 ? 0 :
            (data->n * sizeof(long) * 3) / (time_ms/1000);
        LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
    }
}

replicated global_copy_data data;

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_elements;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 5) {
        LOG("Usage: %s mode log2_num_elements num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_threads = atol(argv[3]);
        args.num_trials = atol(argv[4]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("spawn_mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long) * 2);

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 2 * n, 2 * mbytes, 2 * mbytes_per_nodelet);
    fflush(stdout);
    data.num_threads = args.num_threads;
    global_copy_init(&data, n);
    LOG("Doing copy using %s\n", args.mode); fflush(stdout);

    #define RUN_BENCHMARK(X) global_copy_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(global_copy_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(global_copy_serial_spawn);
    } else if (!strcmp(args.mode, "serial_remote_spawn")) {
        runtime_assert(data.num_threads >= NODELETS(), "serial_remote_spawn mode will always use at least one thread per nodelet");
        RUN_BENCHMARK(global_copy_serial_remote_spawn);
    } else if (!strcmp(args.mode, "serial_remote_spawn_shallow")) {
        runtime_assert(data.num_threads >= NODELETS(), "serial_remote_spawn_shallow mode will always use at least one thread per nodelet");
        RUN_BENCHMARK(global_copy_serial_remote_spawn_shallow);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(global_copy_recursive_spawn);
    } else if (!strcmp(args.mode, "recursive_remote_spawn")) {
        runtime_assert(data.num_threads >= NODELETS(), "recursive_remote_spawn mode will always use at least one thread per nodelet");
        RUN_BENCHMARK(global_copy_recursive_remote_spawn);
    } else if (!strcmp(args.mode, "library")) {
        runtime_assert(data.num_threads >= NODELETS(), "emu_for_2d mode will always use at least one thread per nodelet");
        RUN_BENCHMARK(global_copy_library);
    } else if (!strcmp(args.mode, "serial")) {
        runtime_assert(data.num_threads == 1, "serial mode can only use one thread");
        RUN_BENCHMARK(global_copy_serial);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }
#ifndef NO_VALIDATE
    LOG("Validating results...");
    global_copy_validate(&data);
    LOG("OK\n");
#endif
    global_copy_deinit(&data);
    return 0;
}
