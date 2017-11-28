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
    size_t n;
    T * data;
public:
    emu_striped_array(size_t n) : n(n)
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
    operator[] (size_t i)
    {
        return data[i];
    }

    const T&
    operator[] (size_t i) const
    {
        return data[i];
    }

    // Apply a function to each element of the array in parallel
    template <typename F>
    void
    parallel_apply(long grain, F worker)
    {
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS() && nodelet_id < n; ++nodelet_id) {
            cilk_spawn [=](long * hint) {
                // Use a stride to only touch the elements that are local to this nodelet
                for (long i = nodelet_id; i < n; i += NODELETS()) {
                    worker(data[i]);
                }
            }(&data[nodelet_id]);
        }
    }
};