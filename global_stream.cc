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
#include "emu_2d_array.h"

#ifdef __le64__
extern "C" {
#include <memoryweb.h>
}
#else
#include "memoryweb_x86.h"
#endif

#include <utility>

template <typename T>
struct mirrored : public T
{
    // Wrapper constructor to copy-construct T at each nodelet after running the requested constructor
    template<typename... Args>
    explicit mirrored (Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Get pointer to constructed T
        T* local = static_cast<T*>(mw_get_nth(this, 0));
        // Replicate to each remote nodelet
        for (long i = 1; i < NODELETS(); ++i) {
            T * remote = static_cast<T*>(mw_get_nth(this, i));
            // This calls the copy constructor to initialize remote from local
            new(remote) T(*local);
        }
    }

    // Overrides default new to always allocate replicated storage for instances of this class
    static void *
    operator new(std::size_t sz)
    {
        return mw_mallocrepl(sz);
    }

    // Overrides default delete to safely free replicated storage
    static void
    operator delete(void * ptr)
    {
        mw_free(ptr);
    }
};

struct global_stream
{
    emu_2d_array<long> a;
    emu_2d_array<long> b;
    emu_2d_array<long> c;
    long n;
    long num_threads;

    global_stream(long n) : a(n), b(n), c(n), n(n)
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
};

#define RUN_BENCHMARK(X) \
do {                                                        \
    timer_start();                                          \
    benchmark-> X ();                                         \
    long ticks = timer_stop();                              \
    double bw = timer_calc_bandwidth(ticks, benchmark->n * sizeof(long) * 3); \
    timer_print_bandwidth( #X , bw);                        \
} while (0)

void
runtime_assert(bool condition, const char* message) {
    if (!condition) {
        printf("ERROR: %s\n", message); fflush(stdout);
        exit(1);
    }
}

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
    printf("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 3 * n, 3 * mbytes, 3 * mbytes_per_nodelet);
    fflush(stdout);

    // Create the benchmark struct with replicated storage
    auto * benchmark = new mirrored<global_stream>(n);
    benchmark->num_threads = args.num_threads;
    printf("Doing vector addition using %s\n", args.mode); fflush(stdout);

    if (!strcmp(args.mode, "cilk_for")) {
        RUN_BENCHMARK(add_cilk_for);
    } else if (!strcmp(args.mode, "serial_spawn")) {
        RUN_BENCHMARK(add_serial_spawn);
     } else if (!strcmp(args.mode, "serial_remote_spawn")) {
         runtime_assert(benchmark->num_threads >= NODELETS(), "serial_remote_spawn mode will always use at least one thread per nodelet");
         RUN_BENCHMARK(add_serial_remote_spawn);
    // } else if (!strcmp(args.mode, "serial_remote_spawn_shallow")) {
    //     runtime_assert(benchmark->num_threads >= NODELETS(), "serial_remote_spawn_shallow mode will always use at least one thread per nodelet");
    //     RUN_BENCHMARK(global_stream_add_serial_remote_spawn_shallow);
    } else if (!strcmp(args.mode, "recursive_spawn")) {
        RUN_BENCHMARK(add_recursive_spawn);
    // } else if (!strcmp(args.mode, "recursive_remote_spawn")) {
    //     runtime_assert(benchmark->num_threads >= NODELETS(), "recursive_remote_spawn mode will always use at least one thread per nodelet");
    //     RUN_BENCHMARK(global_stream_add_recursive_remote_spawn);
    } else if (!strcmp(args.mode, "serial")) {
        runtime_assert(benchmark->num_threads == 1, "serial mode can only use one thread");
        RUN_BENCHMARK(add_serial);
    } else {
        printf("Mode %s not implemented!", args.mode);
    }

    delete benchmark;
    return 0;
}
