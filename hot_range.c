#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <emu_c_utils/emu_c_utils.h>

#include "common.h"

enum op_mode {
    OP_REMOTE_WRITE,
    OP_REMOTE_ADD,
    OP_ATOMIC_ADD,
    OP_ATOMIC_CAS,
};

typedef struct hot_range_data {
    // Number of elements
    long n;
    // Number of threads
    long num_threads;
    // Parameters for list initialization
    // For all i, operate on (i + offset) % length
    long offset;
    long length;

    // Operation to perform on each element of the array
    enum op_mode op_mode;

    // Target array for all operations
    long * array;
    // Specifies which array elements should be targeted by each thread
    long * indices;
} hot_range_data;

replicated hot_range_data data;

void
clear_array_worker(long * array, long begin, long end, va_list args)
{
    for (long i = begin; i < end; i += NODELETS()) {
        array[i] = 0;
    }
}

void
hot_range_clear_array(hot_range_data * data)
{
    emu_1d_array_apply(data->array, data->n, GLOBAL_GRAIN_MIN(data->n, 128), clear_array_worker);
}

static inline long
transform_1d_index(long i, long n)
{
    return ((i * NODELETS()) & (n-1)) + ((i * NODELETS()) >> PRIORITY(n));
}

void
index_init_worker(long * indices, long begin, long end, va_list args) {
    const long n = data.n;
    const long offset = data.offset;
    const long length = data.length;
    for (long i = begin; i < end; i += NODELETS()) {
        // Map onto a 'length'-sized chunk of the array, 'offset' elements from the start
        // If offset + length > n, the hot range will be split between the first and last nodelets
        long target = (offset + (i % length)) % n;
        // Transform to account for striped indexing
        target = transform_1d_index(target, n);
        indices[i] = target;
    }
}

void
hot_range_data_init(hot_range_data * data, long n, enum op_mode op_mode, long offset, long length, long num_threads)
{
    // Initialize parameters
    mw_replicated_init(&data->n, n);
    mw_replicated_init((long*)&data->op_mode, op_mode);
    mw_replicated_init(&data->offset, offset);
    mw_replicated_init(&data->length, length);
    mw_replicated_init(&data->num_threads, num_threads);

    // Allocate arrays
    long * array = mw_malloc1dlong((size_t)n);
    long * indices = mw_malloc1dlong((size_t)n);
    runtime_assert(array && indices, "Failed to allocate array");
    mw_replicated_init((long*)&data->array, (long)array);
    mw_replicated_init((long*)&data->indices, (long)indices);

    // Set up the list
    emu_1d_array_apply(data->indices, data->n, GLOBAL_GRAIN_MIN(data->n, 128),
        index_init_worker
    );

#ifndef NO_VALIDATE
    // Initialize the array with zeros
    hot_range_clear_array(data);
#endif
}

void
hot_range_remote_write_worker(long * array, long begin, long end, va_list args)
{
    long * indices = data.indices;
    for (long i = begin; i < end; i += NODELETS()) {
        array[indices[i]] = 1;
    }
}

void
hot_range_remote_add_worker(long * array, long begin, long end, va_list args)
{
    long * indices = data.indices;
    for (long i = begin; i < end; i += NODELETS()) {
        REMOTE_ADD(&array[indices[i]], 1);
    }
}

void
hot_range_atomic_add_worker(long * array, long begin, long end, va_list args)
{
    long * indices = data.indices;
    for (long i = begin; i < end; i += NODELETS()) {
        ATOMIC_ADDMS(&array[indices[i]], 1);
    }
}

void
hot_range_atomic_cas_worker(long * array, long begin, long end, va_list args)
{
    long * indices = data.indices;
    for (long i = begin; i < end; i += NODELETS()) {
        long oldval, newval;
        long * target = &array[indices[i]];
        do {
            oldval = *target;
            newval = oldval + 1;
        } while (ATOMIC_CAS(target, newval, oldval) != oldval);
    }
}

void
hot_range_launch(hot_range_data * data)
{
    long grain = data->n / data->num_threads;
    void (*worker_ptr)(long *, long, long, va_list);
    switch (data->op_mode) {
        case OP_ATOMIC_ADD: worker_ptr = hot_range_atomic_add_worker; break;
        case OP_ATOMIC_CAS: worker_ptr = hot_range_atomic_cas_worker; break;
        case OP_REMOTE_ADD: worker_ptr = hot_range_remote_add_worker; break;
        case OP_REMOTE_WRITE: worker_ptr = hot_range_remote_write_worker; break;
        default: assert(0);
    }
    emu_1d_array_apply(data->array, data->n, grain, worker_ptr);
}

void
hot_range_data_deinit(hot_range_data * data)
{
    mw_free(data->array);
    mw_free(data->indices);
}

void
check_value(long i, long actual, long expected)
{
    if (actual != expected) {
        LOG("Error in validation, array[%li] was %li, expected %li\n", i, actual, expected);
        exit(1);
    }
}

void
validate_worker(long * array, long begin, long end, va_list args)
{
    long n = data.n;
    long hot_range_begin = data.offset;
    long hot_range_end = data.offset + data.length;
    long hot_range_length = hot_range_end - hot_range_begin;

    for (long i = begin; i < end; i += NODELETS()) {
        long expected_value;
        if (i < hot_range_begin || i >= hot_range_end) {
            // Values outside the hot range should not be touched
            expected_value = 0;
        } else  {
            if (data.op_mode == OP_REMOTE_WRITE) {
                // Remote writes don't accumulate, each hot value is 1
                expected_value = 1;
            } else {
                // Each value will have been incremented n / length times
                expected_value = n / hot_range_length;
                // If it doesn't divide evenly, some items will be incremented more than others
                if ((i - hot_range_begin) < (n % hot_range_length)) {
                    expected_value += 1;
                }
            }
        }
        long physical_index = transform_1d_index(i, n);
        check_value(i, array[physical_index], expected_value);
    }
}

void
hot_range_validate(hot_range_data * data)
{
    emu_1d_array_apply(data->array, data->n, GLOBAL_GRAIN_MIN(data->n, 128), validate_worker);
}

void hot_range_run(hot_range_data * data, long num_trials)
{
    for (long trial = 0; trial < num_trials; ++trial) {
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("hot_range");
        hot_range_launch(data);
        double time_ms = hooks_region_end();
#ifndef NO_VALIDATE
        hot_range_validate(data);
        hot_range_clear_array(data);
#endif
        double ops_per_second = time_ms == 0 ? 0 :
            (data->n) / (time_ms/1000);
        LOG("%3.2f million operations per second\n", ops_per_second / (1000000));
    }
}

static const struct option long_options[] = {
    {"log2_num_elements" , required_argument},
    {"num_threads"       , required_argument},
    {"op_mode"           , required_argument},
    {"log2_offset"       , required_argument},
    {"log2_length"       , required_argument},
    {"num_trials"        , required_argument},
    {"help"              , no_argument},
    {NULL}
};

static void
print_help(const char* argv0)
{
    LOG( "Usage: %s [OPTIONS]\n", argv0);
    LOG("\t--log2_num_elements  Number of elements in the array\n");
    LOG("\t--num_threads        Number of threads scanning the array\n");
    LOG("\t--op_mode            Which operation to do on each element (REMOTE_WRITE, REMOTE_ADD, or ATOMIC_ADD)\n");
    LOG("\t--log2_offset        Offset of the hot range from the beginning of the array\n");
    LOG("\t--log2_length        Number of elements in the hot range.\n");
    LOG("\t--num_trials         Number of times to repeat the benchmark\n");
    LOG("\t--help               Print command line help\n");
}

typedef struct hot_range_args {
    long log2_num_elements;
    long num_threads;
    const char* op_mode;
    long log2_offset;
    long log2_length;
    long num_trials;
} hot_range_args;

static struct hot_range_args
parse_args(int argc, char *argv[])
{
    hot_range_args args;
    args.log2_num_elements = -1;
    args.num_threads = -1;
    args.op_mode = "OP_REMOTE_ADD";
    args.log2_offset = 0;
    args.log2_length = -1;
    args.num_trials = 1;

    int option_index;
    while (true)
    {
        int c = getopt_long(argc, argv, "", long_options, &option_index);
        // Done parsing
        if (c == -1) { break; }
        // Parse error
        if (c == '?') {
            LOG( "Invalid arguments\n");
            print_help(argv[0]);
            exit(1);
        }
        const char* option_name = long_options[option_index].name;

        if (!strcmp(option_name, "log2_num_elements")) {
            args.log2_num_elements = atol(optarg);
        } else if (!strcmp(option_name, "num_threads")) {
            args.num_threads = atol(optarg);
        } else if (!strcmp(option_name, "op_mode")) {
            args.op_mode = optarg;
        } else if (!strcmp(option_name, "log2_offset")) {
            args.log2_offset = atol(optarg);
        } else if (!strcmp(option_name, "log2_length")) {
            args.log2_length = atol(optarg);
        } else if (!strcmp(option_name, "num_trials")) {
            args.num_trials = atol(optarg);
        } else if (!strcmp(option_name, "help")) {
            print_help(argv[0]);
            exit(1);
        }
    }
    if (args.log2_num_elements < 0) { LOG( "log2_num_elements must be >= 0"); exit(1); }
    if (args.num_threads <= 0) { LOG( "num_threads must be > 0"); exit(1); }
    if (args.log2_offset <  0) { LOG( "log2_offset must be >= 0"); exit(1); }
    if (args.log2_length <  0) { LOG( "log2_length must be >= 0"); exit(1); }
    if (args.log2_offset >= args.log2_num_elements) { LOG( "log2_offset must be < log2_num_elements"); exit(1); }
    if (args.log2_length >  args.log2_num_elements) { LOG( "log2_length must be <= log2_num_elements"); exit(1); }
    return args;
}

int main(int argc, char** argv)
{
    const char* active_region = getenv("HOOKS_ACTIVE_REGION");
    if (active_region != NULL) {
        hooks_set_active_region(active_region);
    }

    hot_range_args args = parse_args(argc, argv);

    enum op_mode op_mode;
    if (!strcmp(args.op_mode, "REMOTE_ADD")) {
        op_mode = OP_REMOTE_ADD;
    } else if (!strcmp(args.op_mode, "ATOMIC_ADD")) {
        op_mode = OP_ATOMIC_ADD;
    } else if (!strcmp(args.op_mode, "ATOMIC_CAS")) {
        op_mode = OP_ATOMIC_CAS;
    } else if (!strcmp(args.op_mode, "REMOTE_WRITE")) {
        op_mode = OP_REMOTE_WRITE;
    } else {
        LOG( "Operation %s not implemented!\n", args.op_mode);
        exit(1);
    }

    hooks_set_attr_i64("num_threads", args.num_threads);
    hooks_set_attr_i64("log2_num_elements", args.log2_num_elements);
    hooks_set_attr_i64("log2_offset", args.log2_offset);
    hooks_set_attr_i64("log2_length", args.log2_length);
    hooks_set_attr_str("op_mode", args.op_mode);
    hooks_set_attr_i64("num_nodelets", NODELETS());

    long n = 1L << args.log2_num_elements;
    long offset = 1L << args.log2_offset;
    long length = 1L << args.log2_length;
    LOG("Initializing array...\n")

    hooks_region_begin("init");
    hot_range_data_init(&data, n, op_mode, offset, length, args.num_threads);
    hooks_region_end();

    LOG("Spawning %li threads to do a total of 2^%li %s operations on an array of 2^%li elements with an offset of 2^%li...\n",
        args.num_threads,
        args.log2_num_elements,
        args.op_mode,
        args.log2_length,
        args.log2_offset);

    hot_range_run(&data,args.num_trials);

    hot_range_data_deinit(&data);
    return 0;
}
