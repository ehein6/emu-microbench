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
#include <cassert>
#include <algorithm>
#include <cmath>

template<typename T>
class emu_2d_array {

private:
    size_t n;
    size_t chunk_size;
    T ** data;
public:
    typedef T value_type;

    // Default constructor
    emu_2d_array() : n(0), chunk_size(0), data(nullptr) {}

    // Constructor
    emu_2d_array(size_t num_elements) : n(num_elements)
    {
        // round N up to power of 2 for efficient indexing
        assert(n > 1);
        n = 1 << (PRIORITY(n-1)+1);

        chunk_size = n / NODELETS();
        // Allocate 2D array
        void * raw = mw_malloc2d(NODELETS(), sizeof(T) * chunk_size);
        data = reinterpret_cast<T**>(raw);
        // Call constructor on each element if required
        // TODO do this with parallel macro
        if (!std::is_trivially_default_constructible<T>::value) {
            // Call default constructor (placement-new) on each element using parallel apply
            this->parallel_apply([this](size_t i){
                new(&this->operator[](i)) T();
            });
        }

    }

    // Destructor
    ~emu_2d_array()
    {
        if (data == nullptr) { return; }
        // Call destructor on each element if required
        // TODO do this with parallel macro
        if (!std::is_trivially_destructible<T>::value) {
            this->parallel_apply([this](size_t i){
                this->operator[](i).~T();
            });
        }

        // Free 2D array
        mw_free(data);
    }

    // TODO deleting copy constructor and assignment operator for now, we probably don't want to call these anyways
    emu_2d_array(const emu_2d_array & other) = delete;
    emu_2d_array& operator= (const emu_2d_array &other) = delete;

    // Move constructor
    emu_2d_array(const emu_2d_array && other)
    : n(other.n), chunk_size(other.chunk_size), data(other.data)
    {
        other.data = nullptr;
    }

    // Move assignment
    emu_2d_array& operator= (emu_2d_array &&other)
    {
        if (this != &other) {
            n = other.n;
            chunk_size = other.chunk_size;
            data = other.data;
            other.data = nullptr;
        }
        return *this;
    }

    T&
    operator[] (size_t i)
    {
        assert(data);
        assert(i < n);
        // data[i / chunk_size][i % chunk_size]
        return data[i >> PRIORITY(chunk_size)][i&(chunk_size-1)];
    }

    const T&
    operator[] (size_t i) const
    {
        assert(data);
        assert(i < n);
        // data[i / chunk_size][i % chunk_size]
        return data[i >> PRIORITY(chunk_size)][i&(chunk_size-1)];
    }

    size_t get_size() const { return n; }
    size_t get_chunk_size() const { return chunk_size; }
    T** get_data() { return data; }

private:
    template <typename F>
    static void
    serial_spawn_at_nodelets(long low, long high, long grain, void * hint, F func)
    {
        // Unused pointer parameter makes spawn occur at remote nodelet
        (void)hint;
        // Spawn threads to handle local elements
        local_serial_spawn(low, high, grain, func);
    }
public:
    // Apply a function to each element of the array in parallel, using serial spawn
    template <typename F>
    void
    parallel_apply_serial_spawn(long grain, F func)
    {
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id) {
            long begin = nodelet_id * chunk_size;
            long end = (nodelet_id+1) * chunk_size;
            cilk_spawn serial_spawn_at_nodelets(begin, end, grain, data[nodelet_id], func);
        }
    }
private:

    // Emu will remote spawn a function if the first pointer argument points to a remote nodelet.
    // In the case of a class method, `this` is always the first pointer argument.
    // Instances of emu_2d_array are meant to be replicated, which means `this` will be a relative pointer that doesn't trigger a remote spawn.
    // As a workaround, spawn a static method that accepts the `this` pointer as a parameter.
    // The hint comes before the `this` pointer so remote spawn can work.
    template <typename F>
    static noinline void
    recursive_spawn_at_nodelets(long low, long high, long grain, void * hint, emu_2d_array<T>* self, F func)
    {
        for (;;) {
            long count = high - low;
            if (count == 1) break;
            long mid = low + count / 2;
            cilk_spawn recursive_spawn_at_nodelets(low, mid, grain, self->data[mid], self, func);
            low = mid;
        }

        long nodelet_id = low;
        long begin = nodelet_id * self->chunk_size;
        long end = (nodelet_id+1) * self->chunk_size;
        local_recursive_spawn(begin, end, grain, func);
    }

public:
    // Apply a function to each element of the array in parallel, using recursive spawn
    template <typename F>
    void
    parallel_apply_recursive_spawn(long grain, F func)
    {
        recursive_spawn_at_nodelets(0, NODELETS(), grain, nullptr, this, func);
    }

    // Apply a function to each element of the array in parallel
    template <typename F>
    void
    parallel_apply(F func, long grain=0)
    {
        if (grain == 0) { grain = std::min(2048L, (long)std::ceil(n / 8)); }
        parallel_apply_serial_spawn(grain, func);
    }
};