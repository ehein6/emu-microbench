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

namespace emu {

/**
 * Encapsulates a striped array ( @c mw_malloc1dlong).
 * @tparam T Element type. Must be a 64-bit type (generally @c long or a pointer type).
 */
template<typename T>
class striped_array
{
    static_assert(sizeof(T) == 8, "emu_striped_array can only hold 64-bit data types");
private:
    long n;
    T * data;
    // Default constructor
    striped_array() = default;
public:
    typedef T value_type;
    /**
     * Constructs a emu_striped_array<T>
     * @param n Number of elements
     */
    explicit striped_array(long n) : n(n)
    {
        data = static_cast<T*>(mw_malloc1dlong(static_cast<size_t>(n)));
    }

    // Destructor
    ~striped_array()
    {
        mw_free(data);
    }

    friend void
    swap(striped_array& first, striped_array& second)
    {
        using std::swap;
        swap(first.n, second.n);
        swap(first.data, second.data);
    }

    // Copy constructor
    striped_array(const striped_array & other) : n(other.n)
    {
        // Copy elements over in parallel
        other.parallel_apply([=](long i) {
            data[i] = other[i];
        });
    }

    // Assignment operator (using copy-and-swap idiom)
    striped_array& operator= (striped_array other)
    {
        swap(*this, other);
        return *this;
    }

    // Move constructor (using copy-and-swap idiom)
    striped_array(striped_array&& other) : striped_array()
    {
        swap(*this, other);
    }

    // Shallow copy constructor (used for repl<T>)
    striped_array(const striped_array& other, bool)
    : n(other.n), data(other.data) {}

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

    long size() const { return n; }

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
        if (grain == 0) { grain = std::min(2048L, (long)std::ceil(n / 8)); }
        // Spawn a thread at each nodelet
        for (long nodelet_id = 0; nodelet_id < NODELETS() && nodelet_id < n; ++nodelet_id) {
            cilk_spawn parallel_apply_worker_level1(&data[nodelet_id], n, grain, worker);
        }
    }
};

} // end namespace emu