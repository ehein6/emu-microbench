#pragma once

#include "spawn_templates.h"

extern "C" {
#ifdef __le64__
#include <memoryweb.h>
#else
#include "memoryweb_x86.h"
#endif
}
#include <new>
#include <type_traits>

template<typename T>
class emu_2d_array {

private:
    size_t n;
    size_t block_size;
    T ** data;
public:
    emu_2d_array(size_t n) : n(n)
    {
        // TODO round N up to power of 2
        block_size = n / NODELETS();
        // Allocate 2D array
        void * raw = mw_malloc2d(NODELETS(), sizeof(T) * block_size);
        data = reinterpret_cast<T**>(raw);
        // Call constructor on each element if required
        // TODO do this with parallel macro
        if (!std::is_trivially_default_constructible<T>::value) {
            long grain = 256;
            // Call default constructor (placement-new) on each element using parallel apply
            this->parallel_apply(grain, [this](size_t i){
                new(&this->operator[](i)) T();
            });
        }

    }
    ~emu_2d_array()
    {
        // Call destructor on each element if required
        // TODO do this with parallel macro
        if (!std::is_trivially_destructible<T>::value) {
            long grain = 256;
            this->parallel_apply(grain, [this](size_t i){
                this->operator[](i).~T();
            });
        }

        // Free 2D array
        mw_free(data);
    }

    // TODO deleting copy constructor and assignment operator for now, we probably don't want to call these anyways
    emu_2d_array(const emu_2d_array & other) = delete;
    emu_2d_array& operator= (const emu_2d_array &other) = delete;

    T&
    operator[] (size_t i)
    {
        // data[i / block_size][i % block_size]
        return data[i >> PRIORITY(block_size)][i&(block_size-1)];
    }

    const T&
    operator[] (size_t i) const
    {
        // data[i / block_size][i % block_size]
        return data[i >> PRIORITY(block_size)][i&(block_size-1)];
    }


    // Apply a function to each element of the array in parallel, using serial spawn
    template <typename F>
    void
    parallel_apply_serial_spawn(long grain, F func)
    {
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id) {
            long begin = nodelet_id * block_size;
            long end = (nodelet_id+1) * block_size;

            cilk_spawn [=](T * hint) {
                // Unused pointer parameter makes spawn occur at remote nodelet
                (void)hint;
                // Spawn threads to handle local elements
                local_serial_spawn(begin, end, grain, func);
            }(data[nodelet_id]);
        }
    }
private:
    template <typename F>
    void
    recursive_spawn_at_nodelets(long low, long high, long grain, void * hint, F func)
    {
        for (;;) {
            long count = high - low;
            if (count == 1) break;
            long mid = low + count / 2;
            cilk_spawn recursive_spawn_at_nodelets(low, mid, grain, data[low], func);
            low = mid;
        }

        long nodelet_id = low;
        long begin = nodelet_id * block_size;
        long end = (nodelet_id+1) * block_size;
        local_recursive_spawn(begin, end, grain, func);
    }

public:
    // Apply a function to each element of the array in parallel, using recursive spawn
    template <typename F>
    void
    parallel_apply_recursive_spawn(long grain, F func)
    {
        recursive_spawn_at_nodelets(0, NODELETS(), grain, nullptr, func);
    }

    // Apply a function to each element of the array in parallel
    template <typename F>
    void
    parallel_apply(long grain, F func)
    {
        parallel_apply_serial_spawn(grain, func);
    }
};