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
        // Call constructor on each element
        // TODO do this with parallel macro
        // TODO skip this step if T's default constructor is trivial
//        for (size_t i = 0; i < NODELETS(); ++i) {
//            for (size_t j = 0; j < block_size; ++j) {
//                new (&data[i][j]) T();
//            }
//        }

    }
    ~emu_2d_array()
    {

        // Call destructor on each element
        // TODO do this with parallel macro
        // TODO skip this step if T's destructor is trivial
//        for (size_t i = 0; i < NODELETS(); ++i) {
//            for (size_t j = 0; j < block_size; ++j) {
//                data[i][j].~T();
//            }
//        }

        // Free 2D array
        mw_free(data);
    }

    T&
    operator[] (size_t i)
    {
        // data[i / block_size][i % block_size]
        return data[i >> PRIORITY(block_size)][i&(block_size-1)];
    }

    // Apply a function to each element of the array in parallel
    template <typename F>
    void
    parallel_apply(long grain, F func)
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
};