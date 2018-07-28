#pragma once

extern "C" {
#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif
}
#include <cinttypes>
#include <algorithm>
#include <cmath>
#include "striped_array.h"

namespace emu {

template<typename T>
class jagged_array
{
private:
    // Size of each array such that len(ptrs[i]) == sizes[i]
    striped_array<long> sizes;
    // Array of pointers to arrays of T.
    striped_array<T*> ptrs;

    long n;

    void alloc_local_array(void * hint, long nodelet_id)
    {
        // Scan the offset array to compute the number of elements
        // on this nodelet
        long num_elements = 0;
        for (long i = nodelet_id; i < n; i += NODELETS()) {
            num_elements += sizes[i];
        }
        // Allocate one big chunk for all of them
        size_t size = num_elements * sizeof(T);
        void * buffer = mw_localmalloc(size, hint);
        ptrs[nodelet_id] = static_cast<T*>(buffer);
        // Construct each one
        local_spawn(0, num_elements, [=](long i) {
            new T (ptrs[nodelet_id][i]);
        });
        // Initialize pointers
        T * pos = ptrs[nodelet_id];
        for (long i = nodelet_id; i < n; i += NODELETS()) {
            ptrs[i] = pos;
            pos += sizes[i];
        }
    }

    jagged_array(striped_array<long> sizes)
    : n(sizes.size()), sizes(sizes), ptrs(sizes.size())
    {
        for (long i = 0; i < NODELETS(); ++i) {
            cilk_spawn alloc_local_array(ptrs[i], i);
        }
    }

public:
    typedef T value_type;
    ~jagged_array()
    {
        // TODO destruct elements

        // TODO free in parallel
        for (long i = 0; i < NODELETS(); ++i) {
            mw_localfree(ptrs[i]);
        }
    }

    static jagged_array
    from_offsets(const striped_array<long> & offsets)
    {
        // Make a copy
        striped_array<long> sizes(offsets.size() - 1);
        // Compute the size of each index
        sizes.parallel_apply([=](long i) {
            // TODO could this be done more efficiently with REMOTE_ADD?
            sizes[i] = offsets[i + 1] - offsets[i];
        }
        // Construct jagged array
        return jagged_array(sizes);
    }


    // TODO deleting copy constructor and assignment operator for now, we probably don't want to call these anyways
    striped_array(const striped_array & other) = delete;
    striped_array& operator= (const striped_array &other) = delete;

    T*
    operator[] (long i)
    {
        return data[i];
    }

    const T* operator[] (long i) const
    {
        return data[i];
    }

    private:
    template<typename F>
    static void
    parallel_apply_worker_level2(long begin, long end, F worker)
    {
        // Use a stride to only touch the elements that are local to this nodelet
        const long stride = NODELETS();
        for (long i = begin; i < end; i += stride) {
            worker(i);
        }
    }

    template <typename F>
    static void
    parallel_apply_worker_level1(void * hint, long size, long grain, F worker)
    {
        (void)hint;
        // Spawn threads to handle all the elements on this nodelet
        // Start with an offset of NODE_ID() and a stride of NODELETS()
        long stride = grain*NODELETS();
        for (long i = NODE_ID(); i < size; i += stride) {
            long first = i;
            long last = first + stride; if (last > size) { last = size; }
            cilk_spawn parallel_apply_worker_level2(first, last, worker);
        }
    }
    public:
    /**
     * Applies a function to each element of the array in parallel.
     * @param worker Function to apply to each element. Argument is index of element.
     * @param grain Minimum number of elements to assign to each thread.
     */
    template <typename F>
    void
    parallel_apply(F worker, long grain = 0)
    {
        if (grain == 0) { grain = 256; }
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS() && nodelet_id < n; ++nodelet_id) {
            cilk_spawn parallel_apply_worker_level1(&data[nodelet_id], n, grain, worker);
        }
    }

    // Shallow copy constructor
    striped_array(const striped_array& other, bool)
    : n(other.n), data(other.data) {}
};

} // end namespace emu