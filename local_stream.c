#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include "timer.h"
#include "recursive_spawn.h"

typedef struct local_stream_data {
    long * a;
    long * b;
    long * c;
    long n;
    long num_threads;
} local_stream_data;

void
local_stream_init(local_stream_data * data, long n)
{
    data->n = n;
    data->a = malloc(n * sizeof(long));
    assert(data->a);
    data->b = malloc(n * sizeof(long));
    assert(data->b);
    data->c = malloc(n * sizeof(long));
    assert(data->c);

    cilk_spawn memset(data->a, 0, n * sizeof(long));
    cilk_spawn memset(data->b, 0, n * sizeof(long));
    cilk_spawn memset(data->c, 0, n * sizeof(long));
    cilk_sync;
}

void
local_stream_deinit(local_stream_data * data)
{
    free(data->a);
    free(data->b);
    free(data->c);
}

void
local_stream_add_serial(local_stream_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

void
local_stream_add_cilk_for(local_stream_data * data)
{
    #pragma cilk grainsize = data->n / data->num_threads
    cilk_for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

static void
recursive_spawn_add_worker(long begin, long end, local_stream_data *data)
{
    for (long i = begin; i < end; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

static void
recursive_spawn_add(long begin, long end, long grain, local_stream_data *data)
{
    RECURSIVE_CILK_SPAWN(begin, end, grain, recursive_spawn_add, data);
}

void
local_stream_add_recursive_spawn(local_stream_data * data)
{
    recursive_spawn_add(0, data->n, data->n / data->num_threads, data);
}

void
local_stream_add_serial_spawn(local_stream_data * data)
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
    printf("Initializing arrays with %li elements each (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024)); fflush(stdout);
    local_stream_data data;
    data.num_threads = args.num_threads;
    local_stream_init(&data, n);
    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(local_stream_add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(local_stream_add_serial_spawn);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(local_stream_add_recursive_spawn);
    } else if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(local_stream_add_serial);
    } else {
        printf("Mode %s not implemented!", args.mode);
    }

    local_stream_deinit(&data);
    return 0;
}
