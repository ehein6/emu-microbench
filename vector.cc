#include <cstdlib>
#include <cstdio>
#include <string>
#include <cilk/cilk.h>
#include <emu_c_utils/emu_c_utils.h>
#include "common.h"

namespace emu {

class local_arena
{
protected:
    typedef unsigned char uchar;
    // Value originally returned from mw_mallocrepl
    void * buffer;
    // Pointer to next available byte on this nodelet
    uchar * next_chunk;
    // Size of each stripe, in elements
    size_t size;
public:

    // @param stripe_size: Number of bytes to reserve on each nodelet
    local_arena(size_t stripe_size)
    {
        assert(stripe_size > 0);
        // Allocate a stripe on each nodelet
        void * ptr = mw_mallocrepl(stripe_size);
        assert(ptr); // FIXME
        // Save the pointer in all copies of the allocator
        // since we don't know which one will get freed
        mw_replicated_init((long*)&buffer, (long)ptr);
        mw_replicated_init((long*)&size, (long)stripe_size);

        // Let the nth copy of next_chunk point to the stripe on the nth nodelet
        for (long nlet = 0; nlet < NODELETS(); ++nlet) {
            uchar * nth_chunk = (uchar*)mw_get_nth(buffer, nlet);
            uchar ** nth_ptr = (uchar**)mw_get_nth(&next_chunk, nlet);
            *nth_ptr = nth_chunk;
        }
    }

    ~local_arena()
    {
        mw_free(buffer);
    }

    void *
    allocate(size_t n, const void * hint)
    {
        if (hint) { MIGRATE((void*)hint); }
        // Serial version
        void * ptr = next_chunk;
        next_chunk += n;
        // Parallel-safe version
        // T * ptr = (T*)ATOMIC_ADDMS((long*)next_chunk, (long)(n * sizeof(T)));
        return ptr;
    }
};

// Reserve 2GB on each nodelet for satisfying allocations
extern replicated local_arena g_arena;

template<typename T>
class local_arena_allocator
{
protected:
    local_arena & arena;
public:
    typedef T value_type;

    // Declare friendship with all templated versions of this class
    template<typename U>
    friend class local_arena_allocator;

    // Default constructor, uses the global replicated arena
    local_arena_allocator() : arena(g_arena) {}

    // Alternate constructor for specifying a different arena to use
    explicit
    local_arena_allocator(local_arena & arena) : arena(arena) {}

    // Templated copy-constructor, for use with rebind
    template<typename U>
    local_arena_allocator(const local_arena_allocator<U>& other) : arena(other.arena) {}

    template<typename U>
    bool operator== (const local_arena_allocator<U>& other)
    {
        return arena == other.arena;
    }

    T *
    allocate(size_t n)
    {
        return static_cast<T*>(
            arena.allocate(n * sizeof(T), nullptr)
        );
    }

    T *
    allocate(size_t n, const void * hint)
    {
        return static_cast<T*>(
            arena.allocate(n * sizeof(T), hint)
        );
    }

    void
    deallocate(T *, size_t)
    {
        // Not implemented
    }
};

} // end namespace emu

// Reserve 2GB on each nodelet for satisfying allocations
replicated emu::local_arena emu::g_arena((1UL<<31));

#include <vector>

// Use our custom allocator with std::vector
template<class T>
using vec = std::vector<T, emu::local_arena_allocator<T>>;

// Uncomment to switch back to std::vector
//template<class T>
//using vec = std::vector<T, std::allocator<T>>;


void
worker(vec<long>** vec_array, long tid, long num_iters)
{
    // Do N times...
    for (long i = 0; i < num_iters; ++i){
        // For each nodelet...
        for (long nlet = 0; nlet < NODELETS(); ++nlet) {
            // Each thread gets a private vector on this nodelet
            long index = tid * NODELETS() + nlet;
            // Append a value onto the vector
            // Will occasionally need to resize the vector
            vec_array[index]->push_back(nlet);
        }
    }
}

int main(int argc, char* argv[])
{
    struct {
        long num_threads;
        long num_iters;
    } args;

    if (argc != 3) {
        LOG("Usage: %s num_threads num_iters\n", argv[0]);
        exit(1);
    } else {
        args.num_threads = atol(argv[1]);
        args.num_iters = atol(argv[2]);

        if (args.num_threads <= 0) { LOG("num_threads must be > 0\n"); exit(1); }
        if (args.num_iters <= 0) { LOG("num_iters must be > 0\n"); exit(1); }
    }

    hooks_set_attr_i64("num_iters", args.num_iters);
    hooks_set_attr_i64("num_threads", args.num_threads);

    long n = args.num_threads * NODELETS();

    LOG("Allocating striped array of vectors...\n");
    // Allocate storage for an array of std::vector striped throughout the system
    vec<long>** vec_array = static_cast<vec<long>**>(
        mw_malloc2d(n, sizeof(vec<long>))
    );
    assert(vec_array);
    // Call constructor on each one
    // Make them empty, guaranteeing we will need to resize
    for (long i = 0; i < n; ++i) {
        new(vec_array[i]) vec<long>(0);
    }

    LOG("Spawning %li threads to do %li push_back() operations each\n",
        args.num_threads, args.num_iters);
    hooks_region_begin("push_back");
    for (long tid = 0; tid < args.num_threads; ++tid) {
        cilk_spawn worker(vec_array, tid, args.num_iters);
    }
    cilk_sync;
    hooks_region_end();

#ifndef NO_VALIDATE
    LOG("Checking results...\n");
    // Validate
    bool success = true;
    for (long i = 0; i < n; ++i) {

        if ((long)vec_array[i]->size() != args.num_iters) {
            LOG("Incorrect size! vec[%li]->size() = %li\n",
                i, vec_array[i]->size());
        }
        for (long element : *vec_array[i]) {
            if (element != i % NODELETS()) {
                LOG("Incorrect element! vec[%li] = %li\n",
                    i, element);
                success = false;
                break;
            }
        }
    }
    if (success) { LOG("PASS\n"); }
    else         { LOG("FAIL\n"); }
#endif

    // TODO should call destructor on each vector and mw_free the array

    return 0;
}

