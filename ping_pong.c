#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <hooks.h>

#include "common.h"

#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif

typedef struct ping_pong_data {
    long * a;
    long num_migrations;
    long num_threads;
} ping_pong_data;

void
ping_pong_init(ping_pong_data * data, long num_migrations, long num_threads)
{
    data->num_migrations = num_migrations;
    data->num_threads = num_threads;
    data->a = mw_malloc1dlong(NODELETS());
}

void
ping_pong_deinit(ping_pong_data * data)
{
    mw_free(data->a);
}

// Migrate back and forth between two adjacent nodelets
void
ping_pong_local(ping_pong_data * data)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;
    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[1]);
        MIGRATE(&a[0]);
        MIGRATE(&a[1]);
        MIGRATE(&a[0]);
    }
}

// Migrate back and forth between two adjacent nodes
void
ping_pong_global(ping_pong_data * data)
{
    long * a = data->a;
    // Each iteration forces four migrations
    long n = data->num_migrations / 4;
    for (long i = 0; i < n; ++i) {
        MIGRATE(&a[8]);
        MIGRATE(&a[0]);
        MIGRATE(&a[8]);
        MIGRATE(&a[0]);
    }
}

void
ping_pong_spawn_local(ping_pong_data * data)
{
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn ping_pong_local(data);
    }
}

void
ping_pong_spawn_global(ping_pong_data * data)
{
    runtime_assert(NODELETS() > 8,
        "Global ping pong requires a configuration with more than one node (more than 8 nodelets)"
    );
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn ping_pong_global(data);
    }
}


void ping_pong_run(
    ping_pong_data * data,
    const char * name,
    void (*benchmark)(ping_pong_data *),
    long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin(name);
        benchmark(data);
        double time_ms = hooks_region_end();
        double migrations_per_second = (data->num_migrations) / (time_ms/1e3);
        LOG("%3.2f million migrations per second\n", migrations_per_second / (1e6));
        LOG("Latency (amortized): %3.2f us\n", (1.0 / migrations_per_second) * 1e6);
    }
}

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        long log2_num_migrations;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 5) {
        LOG("Usage: %s mode log2_num_migrations num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_migrations = atol(argv[2]);
        args.num_threads = atol(argv[3]);
        args.num_trials = atol(argv[4]);

        if (args.log2_num_migrations <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    long n = 1L << args.log2_num_migrations;
    ping_pong_data data;
    data.num_threads = args.num_threads;
    ping_pong_init(&data, n, args.num_threads);
    LOG("Doing %s ping pong \n", args.mode);

    #define RUN_BENCHMARK(X) ping_pong_run(&data, args.mode, X, args.num_trials)

    if (!strcmp(args.mode, "local")) {
        RUN_BENCHMARK(ping_pong_spawn_local);
    } else if (!strcmp(args.mode, "global")) {
        RUN_BENCHMARK(ping_pong_spawn_global);
    } else {
        LOG("Mode %s not implemented!", args.mode);
    }

    ping_pong_deinit(&data);
    return 0;
}
