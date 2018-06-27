#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <hooks.h>
#include <emu_striped_array.h>
#include "common.h"
#include "spawn_templates.h"
#include "emu_2d_array.h"
#include "mirrored.h"

#ifdef __le64__
extern "C" {
#include <memoryweb.h>
}
#else
#include "memoryweb_x86.h"
#endif

struct benchmark {
    virtual void run(const char * name, long num_trials) = 0;
    virtual ~benchmark() {};
};

template<template <typename> class array_type>
struct global_stream : public benchmark
{
    array_type<long> a;
    array_type<long> b;
    array_type<long> c;
    long n;
    long num_threads;

    global_stream(long n, long num_threads)
    : a(n), b(n), c(n), n(n), num_threads(num_threads)
    {
    }

    // serial - just a regular for loop
    void
    add_serial()
    {
        for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_cilk_for()
    {
        #pragma cilk grainsize = n / num_threads
        cilk_for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_serial_spawn()
    {
        local_serial_spawn(0, n, n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    add_recursive_spawn()
    {
        local_recursive_spawn(0, n, n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    add_serial_remote_spawn()
    {
        c.parallel_apply(n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    add_recursive_remote_spawn()
    {
        c.parallel_apply(n/num_threads, [this](long i) {
            c[i] = a[i] + b[i];
        });
    }

    void
    run(const char * name, long num_trials)
    {
        LOG("In run(%s, %li)", name, num_trials);
        for (long trial = 0; trial < num_trials; ++trial) {
            hooks_set_attr_i64("trial", trial);

            #define RUN_BENCHMARK(X)            \
            do {                                \
                hooks_region_begin(name);       \
                X();                            \
                time_ms = hooks_region_end();   \
            } while(false)

            double time_ms = 0;
            if (!strcmp(name, "cilk_for")) {
                RUN_BENCHMARK(add_cilk_for);
            } else if (!strcmp(name, "serial_spawn")) {
                RUN_BENCHMARK(add_serial_spawn);
            } else if (!strcmp(name, "serial_remote_spawn")) {
                runtime_assert(num_threads >= NODELETS(), "serial_remote_spawn mode will always use at least one thread per nodelet");
                RUN_BENCHMARK(add_serial_remote_spawn);
            } else if (!strcmp(name, "recursive_spawn")) {
                RUN_BENCHMARK(add_recursive_spawn);
            } else if (!strcmp(name, "recursive_remote_spawn")) {
                runtime_assert(num_threads >= NODELETS(), "recursive_remote_spawn mode will always use at least one thread per nodelet");
                RUN_BENCHMARK(add_recursive_remote_spawn);
            } else if (!strcmp(name, "serial")) {
                runtime_assert(num_threads == 1, "serial mode can only use one thread");
                RUN_BENCHMARK(add_serial);
            } else {
                printf("Mode %s not implemented!", name);
                exit(1);
            }

            #undef RUN_BENCHMARK

            double bytes_per_second = time_ms == 0 ? 0 :
                (n * sizeof(long) * 3) / (time_ms/1000);
            LOG("%3.2f MB/s\n", bytes_per_second / (1000000));
        }
    }
    
};

benchmark *
make_benchmark(const char * layout, long n, long num_threads)
{
    if (!strcmp(layout, "striped")) {
        return new mirrored<global_stream<emu_striped_array>>(n, num_threads);
    } else if (!strcmp(layout, "chunked")) {
        return new mirrored<global_stream<emu_2d_array>>(n, num_threads);
    } else {
        printf("Layout %s not implemented!", layout);
        exit(1);
        return nullptr;
    }
}


int main(int argc, char** argv)
{
    struct {
        const char* mode;
        const char* layout;
        long log2_num_elements;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 6) {
        LOG("Usage: %s mode layout log2_num_elements num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.layout = argv[2];
        args.log2_num_elements = atol(argv[3]);
        args.num_threads = atol(argv[4]);
        args.num_trials = atol(argv[5]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("spawn_mode", args.mode);
    hooks_set_attr_str("layout", args.layout);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long) * 3);

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 3 * n, 3 * mbytes, 3 * mbytes_per_nodelet);
    fflush(stdout);

    // Create the benchmark struct with replicated storage

    auto * benchmark = make_benchmark(args.layout, n, args.num_threads);
    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    benchmark->run(args.mode, args.num_trials);

    delete benchmark;
    return 0;
}
