#pragma once

extern "C" {
#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif
}
#include <cinttypes>

template<typename T>
class emu_striped_array
{
    static_assert(sizeof(T) == 8, "emu_striped_array can only hold 64-bit data types");
private:
    long n;
    T * data;
public:
    typedef T value_type;
    emu_striped_array(long n) : n(n)
    {
        data = reinterpret_cast<T*>(mw_malloc1dlong(n));
    }
    ~emu_striped_array()
    {
        mw_free(data);
    }

    // TODO deleting copy constructor and assignment operator for now, we probably don't want to call these anyways
    emu_striped_array(const emu_striped_array & other) = delete;
    emu_striped_array& operator= (const emu_striped_array &other) = delete;

    T&
    operator[] (long i)
    {
        return data[i];
    }

    const T&
    operator[] (long i) const
    {
        return data[i];
    }


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

    // Apply a function to each element of the array in parallel
    template <typename F>
    void
    parallel_apply(long grain, F worker)
    {
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS() && nodelet_id < n; ++nodelet_id) {
            cilk_spawn parallel_apply_worker_level1(&data[nodelet_id], n, grain, worker);
        }
    }
};