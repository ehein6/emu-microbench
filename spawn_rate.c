#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <emu_c_utils/emu_c_utils.h>

#include "recursive_spawn.h"
#include "common.h"

/*
 * Goal: Quantify the thread spawn overhead in different circumstances:
 * - recursive vs serial spawn tree
 * - number of arguments passed to worker thread
 * - worker thread has stack or is stackless
 * Secondary goal: reproduce issues with va_arg in a minimal test case
 */

typedef struct spawn_rate_data {
    long * array;
    long n;
} spawn_rate_data;

replicated spawn_rate_data data;

#define DO_WORK(begin, end) \
    do {                                        \
        for (long *p = begin; p < end; ++p) {   \
            *p = 1;                             \
        }                                       \
    } while (0)

// Stackless worker function
noinline void
light_worker(long * begin, long * end)
{
    DO_WORK(begin, end);
}

// Delegates to light_worker, so must allocate a stack frame
noinline void
heavy_worker(long * begin, long * end)
{
    light_worker(begin, end);
}

// Recursive spawn, work function is inline
noinline void
recursive_spawn_inline_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count <= grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_inline_worker(begin, mid, grain);
        begin = mid;
    }
    DO_WORK(begin, end);
}

// Recursive spawn, work function is stackless
noinline void
recursive_spawn_light_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count <= grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_light_worker(begin, mid, grain);
        begin = mid;
    }
    light_worker(begin, end);
}

// Recursive spawn, work function has stack
noinline void
recursive_spawn_heavy_worker(long * begin, long * end, long grain)
{
    for (;;) {
        long count = end - begin;
        if (count <= grain) break;
        long * mid = begin + count / 2;
        cilk_spawn recursive_spawn_heavy_worker(begin, mid, grain);
        begin = mid;
    }
    heavy_worker(begin, end);
}

// Serial spawn, work function is stackless
noinline void
serial_spawn_light_worker(long * begin, long * end, long grain)
{
    for (long * first = begin; first < end; first += grain) {
        long * last = first + grain <= end ? first + grain : end;
        cilk_spawn light_worker(first, last);
    }
}

// Recursive spawn, work function has stack
noinline void
serial_spawn_heavy_worker(long * begin, long * end, long grain)
{
    for (long * first = begin; first < end; first += grain) {
        long * last = first + grain <= end ? first + grain : end;
        cilk_spawn heavy_worker(first, last);
    }
}

// Worker used by emu_c_utils
noinline void
library_inline_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    DO_WORK(first, last);
}

noinline void
library_light_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    light_worker(first, last);
}

noinline void
library_heavy_worker(long begin, long end, va_list args)
{
    // NOTE begin and end are passed as indices, not pointers
    long* array = va_arg(args, long*);
    long * first = array + begin;
    long * last = array + end;
    heavy_worker(first, last);
}




void
init(long n)
{
    data.n = n;
    data.array = mw_localmalloc(n * sizeof(long), &n);
    assert(data.array);
}

void
deinit()
{
    mw_localfree(data.array);
}

void
clear()
{
    memset(data.array, 0, data.n * sizeof(long));
}

void
validate()
{
    bool success = true;
    for (long i = 0; i < data.n; ++i) {
        if (data.array[i] != 1) {
            success = false;
            break;
        }
    }
    if (success) {
        LOG("PASSED\n");
    } else {
        LOG("FAILED\n");
        exit(1);
    }
}

// Do all the writes within a single function
noinline void
do_inline()
{
    DO_WORK(data.array, data.array + data.n);
}

// Call light_worker on each element
noinline void
do_light()
{
    long * begin = data.array;
    long * end = data.array + data.n;
    for (long *i = begin; i < end; ++i) {
        light_worker(i, i + 1);
    }
}

// Call heavy_worker on each element
noinline void
do_heavy()
{
    long * begin = data.array;
    long * end = data.array + data.n;
    for (long *i = begin; i < end; ++i) {
        heavy_worker(i, i + 1);
    }
}

// Spawn light_worker for each element
noinline void
do_serial_spawn_light() {
    serial_spawn_light_worker(data.array, data.array + data.n, 1);
}

// Spawn heavy_worker for each element
noinline void
do_serial_spawn_heavy() {
    serial_spawn_heavy_worker(data.array, data.array + data.n, 1);
}

// Recursive spawn tree, leaf threads do work inline
noinline void
do_recursive_spawn_inline() {
    recursive_spawn_inline_worker(data.array, data.array + data.n, 1);
}

// Recursive spawn tree, leaf threads call light_worker
noinline void
do_recursive_spawn_light() {
    recursive_spawn_light_worker(data.array, data.array + data.n, 1);
}

// Recursive spawn tree, leaf threads call heavy_worker
noinline void
do_recursive_spawn_heavy() {
    recursive_spawn_heavy_worker(data.array, data.array + data.n, 1);
}

// Do the work with an emu_c_utils library call
noinline void
do_library_inline()
{
    emu_local_for(0, data.n, 1, library_inline_worker, data.array);
}

noinline void
do_library_light()
{
    emu_local_for(0, data.n, 1, library_light_worker, data.array);
}

noinline void
do_library_heavy()
{
    emu_local_for(0, data.n, 1, library_heavy_worker, data.array);
}


double
run_baseline(const char * name, void (*benchmark)(), long num_trials)
{
    double total_time_ms = 0;
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark();
        total_time_ms += hooks_region_end();
    }
    return total_time_ms / num_trials;
}

void
run_spawn(double serial_time_ms, const char * name, void (*benchmark)(), long num_trials)
{
    // Run the benchmark
    double time_ms = run_baseline(name, benchmark, num_trials);
    // Subtract the time taken to do the work serially
    double spawn_time_ms = time_ms - serial_time_ms;
    // Compute spawn rate in threads per second
    double threads_per_second = spawn_time_ms == 0 ? 0 :
        data.n / (spawn_time_ms/1000);
    // Print result
    LOG("%s: %3.2f million threads/s\n", name, threads_per_second / (1000000));
}

int main(int argc, char** argv)
{
    struct {
        long log2_num_threads;
        long num_trials;
    } args;

    if (argc != 3) {
        LOG("Usage: %s log2_num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.log2_num_threads = atol(argv[1]);
        args.num_trials = atol(argv[2]);

        if (args.log2_num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    long n = 1UL << args.log2_num_threads;
    LOG("Initializing array with %li elements (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024)); fflush(stdout);

    init(n);
    // TODO if validation desired, need to clear after each trial
    clear();

    hooks_set_attr_i64("log2_num_threads", args.log2_num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());

    double inline_time_ms = run_baseline("inline", do_inline, args.num_trials);
    double light_time_ms = run_baseline("light", do_light, args.num_trials);
    double heavy_time_ms = run_baseline("heavy", do_heavy, args.num_trials);

#define RUN_BENCHMARK(SERIAL_TIME, NAME) \
run_spawn(SERIAL_TIME, #NAME, do_##NAME, args.num_trials)

    RUN_BENCHMARK(light_time_ms, serial_spawn_light);
    RUN_BENCHMARK(heavy_time_ms, serial_spawn_heavy);

    RUN_BENCHMARK(inline_time_ms, recursive_spawn_inline);
    RUN_BENCHMARK(light_time_ms, recursive_spawn_light);
    RUN_BENCHMARK(heavy_time_ms, recursive_spawn_heavy);

    RUN_BENCHMARK(inline_time_ms, library_inline);
    RUN_BENCHMARK(light_time_ms, library_light);
    RUN_BENCHMARK(heavy_time_ms, library_heavy);

    deinit();
    return 0;
}
