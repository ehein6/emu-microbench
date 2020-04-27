#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cilk/cilk.h>
#include <cassert>
#include <cstring>

#include <emu_c_utils/emu_c_utils.h>
#include <emu_cxx_utils/for_each.h>
#include "common.h"

using namespace emu;

struct local_stream {
    long * a;
    long * b;
    long * c;
    long n;

    local_stream(long n) : n(n)
    {
        a = (long*)malloc(n * sizeof(long));
        assert(a);
        b = (long*)malloc(n * sizeof(long));
        assert(b);
        c = (long*)malloc(n * sizeof(long));
        assert(c);
    }

    ~local_stream()
    {
        free(a);
        free(b);
        free(c);
    }

    void init()
    {
        memset(a, 0, n * sizeof(long));
        memset(b, 0, n * sizeof(long));
        memset(c, 0, n * sizeof(long));
    }

    void validate()
    {
        for (long i = 0; i < n; ++i) {
            if (c[i] != 3) {
                LOG("VALIDATION ERROR: c[%li] == %li (supposed to be 3)\n", i, c[i]);
                exit(1);
            }
        }
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
#ifndef NO_GRAINSIZE_COMPUTE
        #pragma cilk grainsize = n / num_threads
#endif
        cilk_for (long i = 0; i < n; ++i) {
            c[i] = a[i] + b[i];
        }
    }

    void
    add_sequential()
    {
        parallel::for_each(seq, a, a + n, [this](long& a_ref) {
            long i = &a_ref - a;
            c[i] = a[i] + b[i];
        });
    }

    void
    add_parallel()
    {
        parallel::for_each(par, a, a + n, [this](long& a_ref) {
            long i = &a_ref - a;
            c[i] = a[i] + b[i];
        });
    }

    void
    add_static()
    {
        parallel::for_each(fixed, a, a + n, [this](long& a_ref) {
            long i = &a_ref - a;
            c[i] = a[i] + b[i];
        });
    }

    void
    add_dynamic()
    {
        parallel::for_each(dyn, a, a + n, [this](long& a_ref) {
            long i = &a_ref - a;
            c[i] = a[i] + b[i];
        });
    }

    void
    run(const char * name, long num_trials)
    {
        for (long trial = 0; trial < num_trials; ++trial) {
            hooks_set_attr_i64("trial", trial);

            double time_ms = 0;
            #define RUN_BENCHMARK(X)            \
            do {                                \
                hooks_region_begin(name);       \
                X();                            \
                time_ms = hooks_region_end();   \
            } while(false)

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
        long log2_num_elements;
        long num_trials;
    } args;

    if (argc != 4) {
        LOG("Usage: %s mode log2_num_elements num_trials\n", argv[0]);
        exit(1);
    } else {
        args.mode = argv[1];
        args.log2_num_elements = atol(argv[2]);
        args.num_trials = atol(argv[4]);

        if (args.log2_num_elements <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_str("mode", args.mode);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);

    long n = 1L << args.log2_num_elements;
    LOG("Initializing arrays with %li elements each (%li MiB)\n",
        n, (n * sizeof(long)) / (1024*1024));
    local_stream benchmark(n);
#ifndef NO_VALIDATE
    benchmark.init();
#endif
    LOG("Doing vector addition using %s\n", args.mode);
    benchmark.run(args.mode, args.num_trials);
#ifndef NO_VALIDATE
    benchmark.validate();
#endif
    return 0;
}
