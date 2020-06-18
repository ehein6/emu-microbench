#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <utility>
#include <emu_c_utils/emu_c_utils.h>
#include <emu_cxx_utils/striped_array.h>
#include <emu_cxx_utils/zip_iterator.h>
#include <emu_cxx_utils/replicated.h>
#include <emu_cxx_utils/repl_array.h>
#include <emu_cxx_utils/for_each.h>
#include "common.h"

using namespace emu;

struct global_stream_1d
{
    striped_array<long> a;
    striped_array<long> b;
    striped_array<long> c;
    repl<long> n;

    global_stream_1d(long n)
        : a(n)
        , b(n)
        , c(n)
        , n(n)
    {}

    global_stream_1d(const global_stream_1d& other, emu::shallow_copy shallow)
        : a(other.a, shallow)
        , b(other.b, shallow)
        , c(other.c, shallow)
        , n(other.n)
    {}

    void
    initialize()
    {
        // TODO initialize in parallel
        for (long i = 0; i < n; ++i) {
            a[i] = 1;
            b[i] = 2;
            c[i] = -1;
        }
    }

    void
    validate()
    {
        // TODO check in parallel
        for (long i = 0; i < n; ++i) {
            if (c[i] != 3) {
                LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, c[i]);
                exit(1);
            }
        }
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
        cilk_for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_sequential()
    {
        parallel::for_each(seq, a.begin(), a.end(), [this](long& a_ref) {
            long i = &a_ref - a.begin();
            c[i] = a[i] + b[i];
        });
    }

    void
    add_parallel()
    {
        parallel::for_each(par, a.begin(), a.end(), [this](long& a_ref) {
            long i = &a_ref - a.begin();
            c[i] = a[i] + b[i];
        });
    }

    void
    add_static()
    {
        parallel::for_each(fixed, a.begin(), a.end(), [this](long& a_ref) {
            long i = &a_ref - a.begin();
            c[i] = a[i] + b[i];
        });
    }

    void
    add_dynamic()
    {
        parallel::for_each(dyn, a.begin(), a.end(), [this](long& a_ref) {
            long i = &a_ref - a.begin();
            c[i] = a[i] + b[i];
        });
    }

    void
    transform_static()
    {
        auto begin = make_zip_iterator(a.begin(), b.begin(), c.begin());
        auto end = make_zip_iterator(a.end(), b.end(), c.end());
        parallel::for_each(fixed, begin, end, [](auto t) {
            auto& a = std::get<0>(t);
            auto& b = std::get<1>(t);
            auto& c = std::get<2>(t);
            c = a + b;
        });
    }


    void
    run(const char * name, long num_trials)
    {
        LOG("In run(%s, %li)\n", name, num_trials);
        for (long trial = 0; trial < num_trials; ++trial) {
            hooks_set_attr_i64("trial", trial);

            #define RUN_BENCHMARK(X)            \
            do {                                \
                hooks_region_begin(name);       \
                X();                            \
                time_ms = hooks_region_end();   \
            } while(false)

            double time_ms = 0;
            if (!strcmp(name, "serial")) {
                RUN_BENCHMARK(add_serial);
            } else if (!strcmp(name, "cilk_for")) {
                RUN_BENCHMARK(add_cilk_for);
            } else if (!strcmp(name, "seq")) {
                RUN_BENCHMARK(add_sequential);
            } else if (!strcmp(name, "par")) {
                RUN_BENCHMARK(add_parallel);
            } else if (!strcmp(name, "dyn")) {
                RUN_BENCHMARK(add_dynamic);
            } else if (!strcmp(name, "fixed")) {
                RUN_BENCHMARK(add_static);
            } else if (!strcmp(name, "transform_fixed")) {
                RUN_BENCHMARK(transform_static);
            } else {
                printf("Mode %s not implemented!", name);
                exit(1);
            }

            #undef RUN_BENCHMARK
            double mbytes_per_second = (1e-6 * n * sizeof(long) * 3)
                                       / (1e-3 * time_ms);
            LOG("%3.2f MB/s\n", mbytes_per_second);
        }
    }
};

int main(int argc, char** argv)
{
    struct {
        const char* mode;
        const char* layout;
        long log2_num_elements;
        long num_trials;
    } args;

    if (argc != 4) {
        LOG("Usage: %s mode log2_num_elements num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_trials = atol(argv[3]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("num_nodelets", NODELETS());
    hooks_set_attr_i64("num_bytes_per_element", sizeof(long) * 3);

    long n = 1L << args.log2_num_elements;
    long mbytes = n * sizeof(long) / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing arrays with %li elements each (%li MiB total, %li MiB per nodelet)\n", 3 * n, 3 * mbytes, 3 * mbytes_per_nodelet);
    auto benchmark = emu::make_repl_shallow<global_stream_1d>(n);
#ifndef NO_VALIDATE
    benchmark->initialize();
#endif
    LOG("Doing vector addition using %s\n", args.mode);
    benchmark->run(args.mode, args.num_trials);
#ifndef NO_VALIDATE
    LOG("Validating results...");
    benchmark->validate();
    LOG("OK\n");
#endif
    return 0;
}
