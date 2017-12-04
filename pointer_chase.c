#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cilk/cilk.h>
#include <assert.h>
#include <string.h>

#include "timer.h"
#include "recursive_spawn.h"

#ifdef __le64__
#include <memoryweb.h>
#else

// Mimic memoryweb behavior on x86
// TODO eventually move this all to its own header file
#define NODELETS() (8)
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

typedef struct node {
    struct node * next;
    long payload[0];
} node;

enum sort_mode {
    STRIPED, ORDERED, SHUFFLED
} sort_mode;

typedef struct pointer_chase_data {
    long n;
    long payload_size;
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

//https://benpfaff.org/writings/clc/shuffle.html
/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(long *array, size_t n)
{
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            long t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

void
pointer_chase_data_init(pointer_chase_data * data, long n, long payload_size, long num_threads, enum sort_mode sort_mode)
{
    data->n = n;
    data->payload_size = payload_size;
    data->num_threads = num_threads;
    data->sort_mode = sort_mode;
    // Allocate N nodes, striped across nodelets
    data->pool = mw_malloc2d(n, sizeof(node) + payload_size * sizeof(long));
    assert(data->pool);
    // Store a pointer for this thread's head of the list
    data->heads = (node**)mw_malloc1dlong(num_threads);
    assert(data->heads);
    // Make an array with entries 1 through n
    data->indices = malloc(n * sizeof(long));
    assert(data->indices);

#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        pointer_chase_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(pointer_chase_data));
    }
#endif

    switch (data->sort_mode) {
        case STRIPED: {
            // Initialize indices array with 0 to n-1
            // Due to malloc2D address mode, this will result in a striped layout
            for (long i = 0; i < n; ++i) {
                data->indices[i] = i;
            }
        }
        break;
        case ORDERED: {
            // Initialize with striped index pattern (i.e. 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15)
            // This will transform malloc2D address mode to sequential
            long i = 0;
            for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id) {
                for (long k = 0; k < n; k += NODELETS()) {
                    data->indices[i++] = k + nodelet_id;
                }
            }
            assert(i == n);
        }
        break;
        case SHUFFLED: {
            // Initialize indices array with 0 to n-1
            for (long i = 0; i < n; ++i) {
                data->indices[i] = i;
            }
            // Randomly shuffle it
            shuffle(data->indices, n);
        }
        break;
    }

    for (long i = 0; i < data->n; ++i) {
        // String pointers together according to the index
        long a = data->indices[i];
        long b = data->indices[(i + 1) % data->n];
        data->pool[a]->next = data->pool[b];
        // Initialize payload
        for (long j = 0; j < payload_size; ++j) { data->pool[a]->payload[j] = 1; }
    }

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

noinline void
chase_pointers(node * head, long payload_size)
{
    long num_nodes = 0;
    long sum = 0;
    for (node * p = head; p != NULL; p = p->next) {
        num_nodes += 1;
        for (long i = 0; i < payload_size; ++i) {
            sum += p->payload[i];
        }
    }
    printf("Finished traversing %li nodes: sum = %li\n", num_nodes, sum); fflush(stdout);
}

void
pointer_chase_serial_spawn(pointer_chase_data * data)
{
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn chase_pointers(data->heads[i], data->payload_size);
    }
    cilk_sync;
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
    chase_pointers(data->heads[low], data->payload_size);
}

void
pointer_chase_recursive_spawn(pointer_chase_data * data)
{
    pointer_chase_recursive_spawn_worker(0, data->num_threads, data);
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
        printf("ERROR: %s\n", message); fflush(stdout);
        exit(1);
    }
}

typedef struct pointer_chase_args {
    const char* spawn_mode;
    const char* sort_mode;
    long log2_num_elements;
    long payload_size;
    long num_threads;
} pointer_chase_args;

pointer_chase_args
parse_args(int argc, char** argv)
{
    pointer_chase_args args;
    if (argc != 6) {
        printf("Usage: %s spawn_mode sort_mode log2_num_elements payload_size num_threads\n", argv[0]);
        exit(1);
    } else {
        args.spawn_mode = argv[1];
        args.sort_mode = argv[2];
        args.log2_num_elements = atol(argv[3]);
        args.payload_size = atol(argv[4]);
        args.num_threads = atol(argv[5]);

        if (args.log2_num_elements <= 0) { printf("log2_num_elements must be > 0"); exit(1); }
        if (args.payload_size <= 0) { printf("payload_size must be > 0"); exit(1); }
        if (args.num_threads <= 0) { printf("num_threads must be > 0"); exit(1); }
    }
    return args;
}

int main(int argc, char** argv)
{

    pointer_chase_args args = parse_args(argc, argv);

    enum sort_mode sort_mode;
    if (!strcmp(args.sort_mode, "shuffled")) {
        sort_mode = SHUFFLED;
    } else if (!strcmp(args.sort_mode, "ordered")) {
        sort_mode = ORDERED;
    } else if (!strcmp(args.sort_mode, "striped")) {
        sort_mode = STRIPED;
    } else {
        printf("Sort mode %s not implemented!\n", args.sort_mode); fflush(stdout);
        exit(1);
    }

    long n = 1L << args.log2_num_elements;
    long bytes = n * sizeof(node);
    long mbytes = bytes / (1024*1024);
    long mbytes_per_nodelet = mbytes / NODELETS();
    printf("Initializing %s array with %li elements (%li MiB total, %li MiB per nodelet)\n",
        args.sort_mode, n, mbytes, mbytes_per_nodelet);
    fflush(stdout);
    pointer_chase_data_init(&data,
        n, args.payload_size, args.num_threads, sort_mode);
    printf("Launching %s with %li threads...\n", args.spawn_mode, args.num_threads); fflush(stdout);

    if (!strcmp(args.spawn_mode, "serial_spawn")) {
        RUN_BENCHMARK(pointer_chase_serial_spawn);
    } else if (!strcmp(args.spawn_mode, "recursive_spawn")) {
        RUN_BENCHMARK(pointer_chase_recursive_spawn);
    } else {
        printf("Spawn mode %s not implemented!", args.spawn_mode);
        exit(1);
    }

    pointer_chase_data_deinit(&data);
    return 0;
}
