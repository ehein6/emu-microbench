#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>

#include "timer.h"
#include "emu_for_2d.h"
#include "emu_for_local.h"

#ifdef __le64__
#include <memoryweb.h>
#else

// Mimic memoryweb behavior on x86
// TODO eventually move this all to its own header file
#define NODELETS() (1)
#define NODE_ID() (0)
#include <cilk/cilk_api.h>
#define THREAD_ID() (__cilkrts_get_worker_number())
#define replicated
#define PRIORITY(X) (63-__builtin_clzl(X))
#define noinline __attribute__ ((noinline))
void *
mw_malloc2d(size_t nelem, size_t sz)
{
    // We need an 8-byte pointer for each element, plus the array of elements
    size_t bytes = nelem * sizeof(long) + nelem * sz;
    unsigned char ** ptrs = malloc(bytes);
    if (!ptrs) return NULL;
    // Skip past the pointers to get to the raw array
    unsigned char * data = (unsigned char *)ptrs + nelem * sizeof(long);
    // Assign pointer to each element
    for (size_t i = 0; i < nelem; ++i) {
        ptrs[i] = data + i * sz;
    }
    return ptrs;
}

void *
mw_malloc1dlong(size_t nelem)
{
    return malloc(nelem * sizeof(long));
}

void
mw_free(void * ptr)
{
    free(ptr);
}

#endif

#define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);

typedef struct node {
    struct node * next;
    long weight;
} node;

enum sort_mode {
    ORDERED,
    INTRA_BLOCK_SHUFFLE,
    BLOCK_SHUFFLE,
    FULL_BLOCK_SHUFFLE
} sort_mode;

typedef struct pointer_chase_data {
    long n;
    long block_size;
    long num_threads;
    enum sort_mode sort_mode;
    // One pointer per thread
    node ** heads;
    // Actual array pointer
    node ** pool;
    // Ordering of linked list nodes
    long * indices;
} pointer_chase_data;

replicated pointer_chase_data data;

#define LCG_MUL64 6364136223846793005ULL
#define LCG_ADD64 1

void
lcg_init(unsigned long * x, unsigned long step)
{
    unsigned long mul_k, add_k, ran, un;

    mul_k = LCG_MUL64;
    add_k = LCG_ADD64;

    ran = 1;
    for (un = step; un; un >>= 1) {
        if (un & 1)
            ran = mul_k * ran + add_k;
        add_k *= (mul_k + 1);
        mul_k *= mul_k;
    }

    *x = ran;
}

unsigned long
lcg_rand(unsigned long * x) {
    *x = LCG_MUL64 * *x + LCG_ADD64;
    return *x;
}

//https://benpfaff.org/writings/clc/shuffle.html
/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(long *array, size_t n)
{
    unsigned long rand_state;
    lcg_init(&rand_state, (unsigned long)array);
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
            size_t j = i + lcg_rand(&rand_state) / (ULONG_MAX / (n - i) + 1);
            long t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

// Initializes a list with 0, 1, 2, ...
noinline void
index_init_worker(long begin, long end, void * arg1)
{
    long * list = (long*) arg1;
    for (long i = begin; i < end; ++i) {
        list[i] = i;
    }
}

// Initializes a list with  0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
// This will transform malloc2D address mode to sequential
noinline void
strided_index_init_worker(long begin, long end, void * arg1, void * arg2)
{
    long * list = (long*) arg1;
    long n = (long) arg2;
    long num_nodelets = NODELETS();
    for (long i = begin; i < end; ++i) {
        // TODO strength reduction here
        list[i] = (i * num_nodelets) % n + (i * num_nodelets) / n;
    }
}

// Links the nodes of the list according to the index array
noinline void
relink_worker(long begin, long end, void * arg1)
{
    pointer_chase_data* data = (pointer_chase_data *)arg1;
    for (long i = begin; i < end; ++i) {
        // String pointers together according to the index
        long a = data->indices[i];
        long b = data->indices[i == data->n - 1 ? 0 : i + 1];
        data->pool[a]->next = data->pool[b];
        // Initialize payload
        data->pool[a]->weight = 1;
    }
}

noinline void
memcpy_long_worker(long begin, long end, void * arg1, void * arg2)
{
    long * dst = (long*) arg1;
    long * src = (long*) arg2;
    memcpy(dst + begin, src + begin, (end-begin) * sizeof(long));
}

// Shuffles the index array at a block level
noinline void
block_shuffle_worker(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
{
    long * block_indices = (long*)arg1;
    long * old_indices = (long*)arg2;
    long * new_indices = (long*)arg3;
    long block_size = (long)arg4;

    for (long src_block = begin; src_block < end; ++src_block) {
        long dst_block = block_indices[src_block];
        long * dst_block_ptr = new_indices + dst_block * block_size;
        long * src_block_ptr = old_indices + src_block * block_size;
        // memcpy(dst_block_ptr, src_block_ptr, block_size * sizeof(long));
        for (long i = 0; i < block_size; ++i) {
            dst_block_ptr[i] = src_block_ptr[i];
        }
    }

}


// Shuffles the index array within each block
noinline void
intra_block_shuffle_worker(long begin, long end, void * arg1, void * arg2)
{
    pointer_chase_data* data = (pointer_chase_data *)arg1;
    long block_size = (long)arg2;
    for (long block_id = begin; block_id < end; ++block_id) {
        shuffle(data->indices + block_id * block_size, block_size);
    }
}


void
pointer_chase_data_init(pointer_chase_data * data, long n, long block_size, long num_threads, enum sort_mode sort_mode)
{
    data->n = n;
    data->block_size = block_size;
    data->num_threads = num_threads;
    data->sort_mode = sort_mode;
    // Allocate N nodes, striped across nodelets
    data->pool = mw_malloc2d(n, sizeof(node));
    assert(data->pool);
    // Store a pointer for this thread's head of the list
    data->heads = (node**)mw_malloc1dlong(num_threads);
    assert(data->heads);
    // Make an array with entries 1 through n
    data->indices = malloc(n * sizeof(long));
    assert(data->indices);

    LOG("Replicating pointers...\n");
#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        pointer_chase_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(pointer_chase_data));
    }
#endif

    // Initialize with striped index pattern (i.e. 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15)
    // This will transform malloc2D address mode to sequential
    LOG("Initializing indices...\n");
    emu_local_for_v2(0, n, LOCAL_GRAIN(n),
        strided_index_init_worker, data->indices, (void*)n
    );

    bool do_block_shuffle = false, do_intra_block_shuffle = false;
    switch (data->sort_mode) {
        case ORDERED:
            do_block_shuffle = false;
            do_intra_block_shuffle = false;
            break;
        case INTRA_BLOCK_SHUFFLE:
            do_block_shuffle = false;
            do_intra_block_shuffle = true;
            break;
        case BLOCK_SHUFFLE:
            do_block_shuffle = true;
            do_intra_block_shuffle = false;
            break;
        case FULL_BLOCK_SHUFFLE:
            do_block_shuffle = true;
            do_intra_block_shuffle = true;
            break;
    }


    assert(n % block_size == 0);
    long num_blocks = n / block_size;

    if (do_block_shuffle) {
        LOG("Beginning block shuffle...\n");

        // Make an array with an element for each block
        long * block_indices = malloc(sizeof(long) * num_blocks);
        emu_local_for_v1(0, num_blocks, LOCAL_GRAIN(num_blocks),
            index_init_worker, block_indices
        );

        LOG("shuffle block_indices...\n");
        // Randomly shuffle it
        shuffle(block_indices, num_blocks);

        LOG("copy old_indices...\n");
        // Make a copy of the indices array
        long * old_indices = malloc(sizeof(long) * n);
        emu_local_for_v2(0, n, LOCAL_GRAIN(n),
            memcpy_long_worker, old_indices, data->indices
        );

        LOG("apply block_indices to indices...\n");
        emu_local_for_v4(0, num_blocks, LOCAL_GRAIN(num_blocks),
            block_shuffle_worker, block_indices, old_indices, data->indices, (void*)block_size
        );

        // Clean up
        free(block_indices);
        free(old_indices);
    }

    if (do_intra_block_shuffle) {
        LOG("Beginning intra-block shuffle\n");
        emu_local_for_v2(0, num_blocks, LOCAL_GRAIN(num_blocks),
            intra_block_shuffle_worker, data, (void*)block_size
        );
    }

    LOG("Linking nodes together...\n");
    // String pointers together according to the index
    emu_local_for_v1(0, data->n, LOCAL_GRAIN(data->n),
        relink_worker, data
    );

    LOG("Chop\n");
    // Chop up the list so there is one chunk per thread
    long chunk_size = n/num_threads;
    for (long i = 0; i < data->num_threads; ++i) {
        long first_index = i * chunk_size;
        long last_index = (i+1) * chunk_size - 1;
        // Store a pointer for this thread's head of the list
        data->heads[i] = data->pool[data->indices[first_index]];
        // Set this thread's tail to null so it knows where to stop
        data->pool[data->indices[last_index]]->next = NULL;
    }
}

void
pointer_chase_data_load(pointer_chase_data * data, long n, long block_size, long num_threads, enum sort_mode sort_mode, const char* path)
{
    data->n = n;
    data->block_size = block_size;
    data->num_threads = num_threads;
    data->sort_mode = sort_mode;
    // Allocate N nodes, striped across nodelets
    data->pool = mw_malloc2d(n, sizeof(node));
    assert(data->pool);
    // Store a pointer for this thread's head of the list
    data->heads = (node**)mw_malloc1dlong(num_threads);
    assert(data->heads);
    // Make an array with entries 1 through n
    data->indices = malloc(n * sizeof(long));
    assert(data->indices);

    LOG("Replicating pointers...\n");
#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        pointer_chase_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(pointer_chase_data));
    }
#endif

    FILE * fp = fopen(path, "rb");
    if (!fp) { LOG("Failed to open %s\n", path); exit(1); }
    size_t rv = fread(data->indices, sizeof(long), (size_t)n, fp);
    if (rv != n) { LOG("Wrong number of bytes in file\n"); exit(1); }
    fclose(fp);
    LOG("Loaded index array from %s\n", path);

    LOG("Linking nodes together...\n");
    // String pointers together according to the index
    emu_local_for_v1(0, data->n, LOCAL_GRAIN(data->n),
        relink_worker, data
    );

    LOG("Chop\n");
    // Chop up the list so there is one chunk per thread
    long chunk_size = n/num_threads;
    for (long i = 0; i < data->num_threads; ++i) {
        long first_index = i * chunk_size;
        long last_index = (i+1) * chunk_size - 1;
        // Store a pointer for this thread's head of the list
        data->heads[i] = data->pool[data->indices[first_index]];
        // Set this thread's tail to null so it knows where to stop
        data->pool[data->indices[last_index]]->next = NULL;
    }
}


void
pointer_chase_data_deinit(pointer_chase_data * data)
{
    mw_free(data->pool);
    mw_free(data->heads);
    mw_free(data->indices);
}

noinline long
chase_pointers(node * head)
{
    long num_nodes = 0;
    long sum = 0;
    for (node * p = head; p != NULL; p = p->next) {
        num_nodes += 1;
        sum += p->weight;
    }
    return sum;
//    printf("Finished traversing %li nodes: sum = %li\n", num_nodes, sum);
}

long
pointer_chase_serial_spawn(pointer_chase_data * data)
{
    long * sums = malloc(sizeof(long) * data->num_threads);
    for (long i = 0; i < data->num_threads; ++i) {
        sums[i] = cilk_spawn chase_pointers(data->heads[i]);
    }
    cilk_sync;
    long sum = 0;
    for (long i = 0; i < data->num_threads; ++i) {
        sum += sums[i];
    }
    LOG("Finished traversing nodes: sum = %li\n", sum);
    return sum;
}

void
pointer_chase_recursive_spawn_worker(long low, long high, pointer_chase_data * data)
{
    for (;;) {
        long count = high - low;
        if (count == 1) break;
        long mid = low + count / 2;
        cilk_spawn pointer_chase_recursive_spawn_worker(low, mid, data);
        low = mid;
    }

    /* Recursive base case: call worker function */
    chase_pointers(data->heads[low]);
}

void
pointer_chase_recursive_spawn(pointer_chase_data * data)
{
    pointer_chase_recursive_spawn_worker(0, data->num_threads, data);
}

noinline void
serial_spawn_local(void * hint, pointer_chase_data * data)
{
    (void)hint;
    // Spawn a thread for each list head located at this nodelet
    // Using striped indexing to avoid migrations
    for (long i = NODE_ID(); i < data->num_threads; i += NODELETS()) {
        cilk_spawn chase_pointers(data->heads[i]);
    }
}

void
pointer_chase_serial_remote_spawn(pointer_chase_data * data)
{
    // Spawn a thread at each nodelet
    for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id ) {
        if (nodelet_id >= data->num_threads) { break; }
        cilk_spawn serial_spawn_local(&data->heads[nodelet_id], data);
    }
}

#define RUN_BENCHMARK(X) \
do {                                                        \
    timer_start();                                          \
    X (&data);                                              \
    long ticks = timer_stop();                              \
    double bw = timer_calc_bandwidth(ticks, bytes);         \
    timer_print_bandwidth( #X , bw);                        \
} while (0)

void
runtime_assert(bool condition, const char* message) {
    if (!condition) {
        LOG("ERROR: %s\n", message);
        exit(1);
    }
}

static const struct option long_options[] = {
    {"log2_num_elements" , required_argument},
    {"num_threads"  , required_argument},
    {"block_size"   , required_argument},
    {"spawn_mode"   , required_argument},
    {"sort_mode"    , required_argument},
    {"dump"         , no_argument},
    {"load"         , no_argument},
    {"help"         , no_argument},
    {NULL}
};

static void
print_help(const char* argv0)
{
    LOG( "Usage: %s [OPTIONS]\n", argv0);
    LOG("\t--log2_num_elements  Number of elements in the list\n");
    LOG("\t--num_threads        Number of threads traversing the list\n");
    LOG("\t--block_size         Number of elements to swap at a time\n");
    LOG("\t--spawn_mode         How to spawn the threads\n");
    LOG("\t--sort_mode          How to shuffle the array\n");
    LOG("\t--dump               Dump index array to disk and exit\n");
    LOG("\t--load               Load index array from disk instead of generating it\n");
    LOG("\t--help               Print command line help\n");
}

typedef struct pointer_chase_args {
    long log2_num_elements;
    long num_threads;
    long block_size;
    const char* spawn_mode;
    const char* sort_mode;
    bool do_dump;
    bool do_load;
} pointer_chase_args;

static struct pointer_chase_args
parse_args(int argc, char *argv[])
{
    pointer_chase_args args;
    args.log2_num_elements = 20;
    args.num_threads = 1;
    args.block_size = 1;
    args.spawn_mode = "serial_spawn";
    args.sort_mode = "block_shuffle";
    args.do_dump = false;
    args.do_load = false;

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
        } else if (!strcmp(option_name, "block_size")) {
            args.block_size = atol(optarg);
        } else if (!strcmp(option_name, "spawn_mode")) {
            args.spawn_mode = optarg;
        } else if (!strcmp(option_name, "sort_mode")) {
            args.sort_mode = optarg;
        } else if (!strcmp(option_name, "dump")) {
            args.do_dump = true;
        } else if (!strcmp(option_name, "load")) {
            args.do_load = true;
        } else if (!strcmp(option_name, "help")) {
            print_help(argv[0]);
            exit(1);
        }
    }
    if (args.log2_num_elements <= 0) { LOG( "log2_num_elements must be > 0"); exit(1); }
    if (args.block_size <= 0) { LOG( "block_size must be > 0"); exit(1); }
    if (args.num_threads <= 0) { LOG( "num_threads must be > 0"); exit(1); }
    return args;
}

int main(int argc, char** argv)
{

    pointer_chase_args args = parse_args(argc, argv);

    enum sort_mode sort_mode;
    if (!strcmp(args.sort_mode, "block_shuffle")) {
        sort_mode = BLOCK_SHUFFLE;
    } else if (!strcmp(args.sort_mode, "ordered")) {
        sort_mode = ORDERED;
    } else if (!strcmp(args.sort_mode, "intra_block_shuffle")) {
        sort_mode = INTRA_BLOCK_SHUFFLE;
    } else if (!strcmp(args.sort_mode, "full_block_shuffle")) {
        sort_mode = FULL_BLOCK_SHUFFLE;
    } else {
        LOG( "Sort mode %s not implemented!\n", args.sort_mode);
        exit(1);
    }

    long n = 1L << args.log2_num_elements;
    long bytes = n * (sizeof(node));
    long mbytes = bytes / (1000000);
    long mbytes_per_nodelet = mbytes / NODELETS();
    LOG("Initializing %s array with %li elements (%li MB total, %li MB per nodelet)\n",
        args.sort_mode, n, mbytes, mbytes_per_nodelet);

    char dump_path[128];
    snprintf(dump_path, 128, "%li.%li.%s", args.log2_num_elements, args.block_size, args.sort_mode);

    // Initialize the array
    if (args.do_load) {
        pointer_chase_data_load(&data,
            n, args.block_size, args.num_threads, sort_mode, dump_path);
    } else {
        pointer_chase_data_init(&data,
            n, args.block_size, args.num_threads, sort_mode);
    }

    // Dump array to disk
    if (args.do_dump) {
        FILE * fp = fopen(dump_path, "wb");
        if (!fp) { LOG("Failed to open %s\n", dump_path); exit(1); }
        size_t rv = fwrite(data.indices, sizeof(long), (size_t)data.n, fp);
        if (rv != data.n) { LOG("Failed to dump\n"); exit(1); }
        fclose(fp);

        LOG("Saved index array to %s, exiting\n", dump_path);
        exit(0);
    }

    LOG( "Launching %s with %li threads...\n", args.spawn_mode, args.num_threads); fflush(stdout);

    if (!strcmp(args.spawn_mode, "serial_spawn")) {
        RUN_BENCHMARK(pointer_chase_serial_spawn);
    } else if (!strcmp(args.spawn_mode, "serial_remote_spawn")) {
        RUN_BENCHMARK(pointer_chase_serial_remote_spawn);
    } else {
        LOG( "Spawn mode %s not implemented!", args.spawn_mode);
        exit(1);
    }

    pointer_chase_data_deinit(&data);
    return 0;
}
