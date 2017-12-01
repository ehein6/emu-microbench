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

#define PAYLOAD_SIZE 1

typedef struct node {
    struct node * next;
    long payload[PAYLOAD_SIZE];
} node;

typedef struct pointer_chase_data {
    long n;
    long num_threads;
    bool do_shuffle;
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
pointer_chase_data_init(pointer_chase_data * data, long n, long num_threads, const char* mode)
{
    if (!strcmp(mode, "shuffled")) {
        data->do_shuffle = true;
    } else if (!strcmp(mode, "unshuffled")) {
        data->do_shuffle = false;
    } else {
        printf("Mode %s not implemented!\n", mode); fflush(stdout);
        exit(1);
    }

    data->n = n;
    data->num_threads = num_threads;
    // Allocate N nodes, striped across nodelets
    data->pool = mw_malloc2d(n, sizeof(node));              assert(data->pool);
    // Store a pointer for this thread's head of the list
    data->heads = (node**)mw_malloc1dlong(num_threads);     assert(data->heads);
    // Make an array with entries 1 through n
    data->indices = malloc(n * sizeof(long));               assert(data->indices);

#ifdef __le64__
    // Replicate pointers to all other nodelets
    data = mw_get_nth(data, 0);
    for (long i = 1; i < NODELETS(); ++i) {
        pointer_chase_data * remote_data = mw_get_nth(data, i);
        memcpy(remote_data, data, sizeof(pointer_chase_data));
    }
#endif

    // Initialize indices array with 0 to n-1
    for (long i = 0; i < n; ++i) {
        data->indices[i] = i;
    }
    // Randomly shuffle it
    if (data->do_shuffle) { shuffle(data->indices, n); }
    // String pointers together according to the index
    for (long i = 0; i < data->n; ++i) {
        long a = data->indices[i];
        long b = data->indices[(i + 1) % data->n];
        data->pool[a]->next = data->pool[b];
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
chase_pointers(node * head)
{
    long total = 1;
    for (node * p = head; p->next != NULL; p = p->next) {
        total += 1;
    }
//    printf("Finished traversing %li nodes\n", total); fflush(stdout);
}

void
pointer_chase_begin(pointer_chase_data * data)
{
    for (long i = 0; i < data->num_threads; ++i) {
        cilk_spawn chase_pointers(data->heads[i]);
    }
    cilk_sync;
}

#define RUN_BENCHMARK(X) \
do {                                                        \
    timer_start();                                          \
    X (&data);                                              \
    long ticks = timer_stop();                              \
    double bw = timer_calc_bandwidth(ticks, data.n * sizeof(long) * 3); \
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
    printf("Initializing %s array with %li elements (%li MiB total, %li MiB per nodelet)\n",
        args.mode, n, mbytes, mbytes_per_nodelet);
    fflush(stdout);
    pointer_chase_data_init(&data, n, args.num_threads, args.mode);
    printf("Chasing pointers with %li threads...\n", args.num_threads); fflush(stdout);

    RUN_BENCHMARK(pointer_chase_begin);

    pointer_chase_data_deinit(&data);
    return 0;
}
