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

namespace emu {

/**
 * Encapsulates a chunked array ( Blocked allocation with @c mw_malloc2d).
 * @tparam T Element type
 */
template<typename T>
class chunked_array {

private:
    long n;
    long chunk_size;
    T ** data;
public:
    typedef T value_type;

    // Default constructor
    chunked_array() : n(0), chunk_size(0), data(nullptr) {}

    /**
     * Constructs an emu_2d_array
     * @param num_elements Number of elements
     */
    explicit chunked_array(long num_elements) : n(num_elements)
    {
        // round N up to power of 2 for efficient indexing
        assert(n > 1);
        n = 1 << (PRIORITY(n-1)+1);

        chunk_size = n / NODELETS();
        // Allocate 2D array
        void * raw = mw_malloc2d(NODELETS(), sizeof(T) * chunk_size);
        data = static_cast<T**>(raw);
        // Call constructor on each element if required
        if (!std::is_trivially_default_constructible<T>::value) {
            // Call default constructor (placement-new) on each element using parallel apply
            this->parallel_apply([this](long i){
                new(&this->operator[](i)) T();
            });
        }

    }

    // Destructor
    ~chunked_array()
    {
        if (data == nullptr) { return; }
        // Call destructor on each element if required
        if (!std::is_trivially_destructible<T>::value) {
            this->parallel_apply([this](long i){
                this->operator[](i).~T();
            });
        }

        // Free 2D array
        mw_free(data);
    }

    friend void
    swap(chunked_array& first, chunked_array& second)
    {
        using std::swap;
        swap(first.n, second.n);
        swap(first.chunk_size, second.chunk_size);
        swap(first.data, second.data);
    }

    // Copy constructor
    chunked_array(const chunked_array & other) : n(other.n), chunk_size(other.chunk_size) {
        other.parallel_apply([=](long i) {
            // TODO call parallel memcpy on each block instead of using index operator
            this->operator[](i) = other[i];
        });
    }

    // Assignment operator (using copy-and-swap idiom)
    chunked_array& operator= (chunked_array other)
    {
        swap(*this, other);
    }

    // Move constructor
    chunked_array(chunked_array && other) noexcept : chunked_array()
    {
        swap(*this, other);
    }

    // Shallow copy constructor (for repl<T>)
    chunked_array(const chunked_array& other, bool)
    : n(other.n), chunk_size(other.chunk_size), data(other.data) {}

    T&
    operator[] (long i)
    {
        assert(data);
        assert(i < n);
        // data[i / chunk_size][i % chunk_size]
        return data[i >> PRIORITY(chunk_size)][i&(chunk_size-1)];
    }

    const T&
    operator[] (long i) const
    {
        assert(data);
        assert(i < n);
        // data[i / chunk_size][i % chunk_size]
        return data[i >> PRIORITY(chunk_size)][i&(chunk_size-1)];
    }

    long get_size() const { return n; }
    long get_chunk_size() const { return chunk_size; }
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
    recursive_spawn_at_nodelets(long low, long high, long grain, void * hint, chunked_array<T>* self, F func)
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

    /**
     * Apply a function to each element of the array in parallel
     * @param func Function to apply to each element. Argument is the index of the array element.
     * @param grain Minimum number of elements to assign to each thread.
     */
    template <typename F>
    void
    parallel_apply(F func, long grain=0)
    {
        if (grain == 0) { grain = std::min(2048L, (long)std::ceil(n / 8)); }
        parallel_apply_serial_spawn(grain, func);
    }
};

} // end namespace emu