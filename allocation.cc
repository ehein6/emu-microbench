#include <cstdlib>
#include <cstdio>
#include <cilk/cilk.h>

#include <emu_c_utils/emu_c_utils.h>

#include "common.h"

#ifndef __EMU_CC__
#define RELEASE(X, Y) abort()
#endif

// Define type-safe wrappers for Emu atomic intrinsics
// TODO Include these from emu_cxx_utils

// Increment 64-bit int
inline long
atomic_addms(volatile long * ptr, long value)
{
    return ATOMIC_ADDMS(ptr, value);
}

// Increment pointer
template<class T>
inline T*
atomic_addms(T * volatile * ptr, ptrdiff_t value)
{
    value *= sizeof(T);
    return (T*)ATOMIC_ADDMS((volatile long*)ptr, (long)value);
}


template<class T>
inline T
atomic_cas(T volatile * ptr, T oldval, T newval);

template<>
inline long
atomic_cas(long volatile * ptr, long oldval, long newval)
{
    return ATOMIC_CAS(ptr, oldval, newval);
}

template<typename T>
inline T
atomic_cas(T volatile * ptr, T oldval, T newval)
{
    static_assert(sizeof(T) == sizeof(long), "CAS supported only for 64-bit types");
    // We're fighting the C++ type system, but codegen doesn't care
    union pun {
        T t;
        long i;
    };
    pun oldval_p{oldval};
    pun newval_p{newval};
    pun retval_p;

    retval_p.i = ATOMIC_CAS(
        reinterpret_cast<long volatile*>(ptr),
        newval_p.i,
        oldval_p.i
    );
    return retval_p.t;
}


// Specialize this for each allocator
template<class Allocator>
Allocator create_allocator(size_t block_size, size_t num_blocks);

/**
 * Uses malloc and free directly
 */
class mallocator {
public:
    // Just use malloc and free
    void * alloc(size_t sz) { return malloc(sz); }
    void dealloc(void * ptr) { free(ptr); }
};
template<>
mallocator
create_allocator(size_t block_size, size_t num_blocks)
{
    return mallocator();
}

/**
 * Pre-allocates a single large buffer, and uses atomic_add to reserve a buffer
 * Nothing is deallocated until the allocator is destroyed
 */
class monotonic_buffer_allocator
{
private:
    uint8_t * buffer;
    uint8_t * pool_begin;
    uint8_t * pool_end;
public:

    monotonic_buffer_allocator(size_t pool_size)
    {
        // Allocate the initial pool using malloc
        buffer = static_cast<uint8_t*>(malloc(pool_size));
        pool_begin = buffer;
        pool_end = pool_begin + pool_size;
    }

    ~monotonic_buffer_allocator()
    {
        // Free the pool
        free(static_cast<void*>(buffer));
    }

    void * alloc(size_t sz)
    {
        // Increment the pointer to reserve our chunk
        uint8_t * ptr = atomic_addms(&pool_begin, sz);
        // Check for overflow
        if (ptr + sz > pool_end) {
            // Runtime malloc error
            RELEASE(1, 3);
        }
        // Return the buffer
        return ptr;
    }

    void dealloc(void * ptr)
    {
        // Do nothing. We deallocate everything at the end
    }
};
template<>
monotonic_buffer_allocator
create_allocator(size_t block_size, size_t num_blocks)
{
    return monotonic_buffer_allocator(block_size * num_blocks);
}

//  free list (CAS loop)
//    Pre-populate a free list with N * M frames. Each thread does CAS on the pointer to claim frames
class free_list_allocator
{
private:
    struct block{
        block * next;
    };
    volatile block * head;
    uint8_t * buffer;
public:

    free_list_allocator(size_t block_size, size_t num_blocks)
    {
        // Allocate a buffer to hold all the blocks
        buffer = static_cast<uint8_t*>(
            malloc(block_size * num_blocks)
        );
        // Head points to the start of the buffer
        head = reinterpret_cast<block*>(buffer);
        // Chop the buffer up into a linked list of blocks
        // Our "block" structure only represents the pointer, so we use
        // raw pointer math to move to the next block
        volatile block * b = head;
        for (size_t i = 1; i < num_blocks; ++i) {
            b->next = reinterpret_cast<block*>(buffer + (i * block_size));
            b = b->next;
        }
        b->next = nullptr;
    }

    ~free_list_allocator() {
        free(buffer);
    }

    void * alloc(size_t sz) {
        // Pointer to the block we're trying to claim
        volatile block *my_block;
        // Pointer to the next free block
        volatile block *next_block;
        do {
            // Read pointer to the head of the list
            my_block = head;
            if (my_block == nullptr) { RELEASE(1, 3); }
            // Read pointer to the next free block
            next_block = my_block->next;
            // Atomically replace the head of the list with the next free block
        } while (my_block != atomic_cas(&head, my_block, next_block));

        return (void*)my_block;
    }

    void dealloc(void * ptr) {
        // TODO put the block back on the free list using CAS
    }
};


template<>
free_list_allocator
create_allocator(size_t block_size, size_t num_blocks)
{
    return free_list_allocator(block_size, num_blocks);
}


template<class Allocator>
void
worker(Allocator& allocator, long block_size, long num_blocks)
{
    // Do N allocations of size sz
    for (long i = 0; i < num_blocks; ++i){
        allocator.alloc(block_size);
    }
}

template<typename Allocator>
void run_test(long block_size, long num_blocks, long num_threads, long num_trials)
{
    long n_per_thread = num_blocks / num_threads;
    for (long trial = 0; trial < num_trials; ++trial) {
        // Re-initialize the allocator for each trial
        auto allocator = create_allocator<Allocator>(block_size, num_blocks);
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("allocation");
        for (long i = 0; i < num_threads; ++i){
            cilk_spawn worker(allocator, block_size, n_per_thread);
        }
        cilk_sync;
        double time_ms = hooks_region_end();
        double mallocs_per_second = num_blocks / (time_ms/1000);
        LOG("%3.2f million allocations per second\n",
            mallocs_per_second / (1000000));
    }
}

int main(int argc, char** argv)
{
    struct {
        long log2_num_mallocs;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 4) {
        LOG("Usage: %s log2_num_mallocs num_threads num_trials\n", argv[0]);
        exit(1);
    } else {
        args.log2_num_mallocs = atol(argv[1]);
        args.num_threads = atol(argv[2]);
        args.num_trials = atol(argv[3]);

        if (args.log2_num_mallocs <= 0) { LOG("log2_num_elements must be > 0"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0"); exit(1); }
    }

    hooks_set_attr_i64("log2_num_mallocs", args.log2_num_mallocs);
    hooks_set_attr_i64("num_threads", args.num_threads);

    long num_blocks = 1L << args.log2_num_mallocs;
    long block_size = 4096;

    LOG("%li threads to do %li malloc/free operations\n",
        args.num_threads, num_blocks);


    LOG("Malloc:\n");
    hooks_set_attr_str("allocator", "mallocator");
    run_test<mallocator>(
        block_size, num_blocks, args.num_threads, args.num_trials);

    LOG("Monotonic buffer (ATOMIC_ADDMS):\n");
    hooks_set_attr_str("allocator", "monotonic_buffer_allocator");
    run_test<monotonic_buffer_allocator>(
        block_size, num_blocks, args.num_threads, args.num_trials);

    LOG("Free list (CAS):\n");
    hooks_set_attr_str("allocator", "free_list_allocator");
    run_test<free_list_allocator>(
        block_size, num_blocks, args.num_threads, args.num_trials);

    return 0;
}
