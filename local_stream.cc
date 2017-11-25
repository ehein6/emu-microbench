#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

extern "C" {
#include "timer.h"
}
#include "spawn_templates.h"

struct local_stream {
    long * a;
    long * b;
    long * c;
    long n;
    long num_threads;

    local_stream(long n) : n(n)
    {
        a = (long*)malloc(n * sizeof(long));
        assert(a);
        b = (long*)malloc(n * sizeof(long));
        assert(b);
        c = (long*)malloc(n * sizeof(long));
        assert(c);

        // TODO init in parallel
        memset(a, 0, n * sizeof(long));
        memset(b, 0, n * sizeof(long));
        memset(c, 0, n * sizeof(long));
    }

    ~local_stream()
    {
        free(a);
        free(b);
        free(c);
    }

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
};


#define RUN_BENCHMARK(X) \
do {                                                        \
    timer_start();                                          \
    benchmark. X ();                                         \
    long ticks = timer_stop();                              \
    double bw = timer_calc_bandwidth(ticks, benchmark.n * sizeof(long) * 3); \
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
        printf("Usage: %s mode log2_num_elements num_threads\n", argv[0]);
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
    local_stream benchmark(n);
    benchmark.num_threads = args.num_threads;

    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(add_serial_spawn);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(add_recursive_spawn);
    } else if (!strcmp(args.mode, "serial")) {
        RUN_BENCHMARK(add_serial);
    } else {
        printf("Mode %s not implemented!", args.mode);
    }

    return 0;
}
