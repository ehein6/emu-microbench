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

    memset(data->a, 0, n * sizeof(long));
    memset(data->b, 0, n * sizeof(long));
    memset(data->c, 0, n * sizeof(long));
}

void
local_stream_deinit(local_stream_data * data)
{
    free(data->a);
    free(data->b);
    free(data->c);
}

void
local_stream_copy_serial(local_stream_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i];
    }
}

void
local_stream_add_serial(local_stream_data * data)
{
    for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i] + data->b[i];
    }
}

void
local_stream_copy_cilk_for(local_stream_data * data)
{
    cilk_for (long i = 0; i < data->n; ++i) {
        data->c[i] = data->a[i];
    }
}

void
local_stream_add_cilk_for(local_stream_data * data)
{
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

    if (argc != 2) {
        printf("Usage: %s num_elements\n", argv[0]);
        exit(1);
    }

    local_stream_data data;
    local_stream_init(&data, atol(argv[1]));
    printf("sizeof(long) == %li\n", sizeof(long));
    printf("Arrays have %li elements (%li MiB)\n", data.n, (data.n * sizeof(long)) / (1024*1024));
    printf("num_threads == 1\n");
    data.num_threads = 1;
    RUN_BENCHMARK(local_stream_add_serial);
    RUN_BENCHMARK(local_stream_add_serial);
    RUN_BENCHMARK(local_stream_add_serial);

    for (data.num_threads = 2; data.num_threads <= 256; data.num_threads *= 2)
    {
        printf("num_threads == %li\n", data.num_threads);
        RUN_BENCHMARK(local_stream_add_cilk_for);
        RUN_BENCHMARK(local_stream_add_cilk_for);
        RUN_BENCHMARK(local_stream_add_cilk_for);
    }

    local_stream_deinit(&data);
    return 0;
}
