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
#include "mirrored.h"

namespace emu {

/**
 * Uses a malloc1d_long to create a CSR-like data structure
 *
 * @tparam T
 */

template<typename T>
class ragged_array
{
    static_assert(sizeof(T) == 8, "ragged_array can only hold 64-bit data types");
private:
    // Size of each array such that len(ptrs[i]) == sizes[i]
    striped_array<T> items;
    striped_array<long> offsets_begin;
    striped_array<long> offsets_end;


    ragged_array(long num_items, striped_array<long>&& offsets_begin, striped_array<long>&& offsets_end)
    : items(num_items)
    , offsets_begin(std::move(offsets_begin))
    , offsets_end(std::move(offsets_end))
    {
    }

    static void
    compute_local_offsets(void * hint, long nodelet_id,
        striped_array<long>& offsets,
        const striped_array<long>& sizes)
    {
        long cum_sum = nodelet_id;
        for (long i = nodelet_id; i < sizes.size(); i += NODELETS()) {
            offsets[i] = cum_sum;
            cum_sum += sizes[i];
        }
        offsets[sizes.size()] = cum_sum;
    }

    // Helper function to compute list of offsets from array of bucket sizes
    static striped_array<long>
    compute_offsets(const striped_array<long> & sizes)
    {
        // Make another array with the same length
        // TODO should be replicated
        striped_array<long> offsets(sizes.size() + 1);
        // TODO need prefix scan here
        for (long nodelet_id = 0; nodelet_id < NODELETS(); ++nodelet_id) {
            cilk_spawn compute_local_offsets(&offsets[nodelet_id], nodelet_id, offsets, sizes);
        }
        cilk_sync;
        return offsets;
    }

    // Helper function to compute end_offsets from list of offsets
    static striped_array<long>
    compute_end_offsets(striped_array<long> & offsets)
    {
        // Make another array with the same length
        // TODO should be replicated
        striped_array<long> end_offsets(offsets.size());
        // Shift all entries to the left
        // Now end_offsets[i] = offsets[i + 1];
        // Doing it this way allows remote writes
        offsets.parallel_apply([&](long i) {
            if (i == 0) return;
            end_offsets[i - 1] = offsets[i];
        });
        return end_offsets;
    }

    static long
    longest_chunk(const striped_array<long>& offsets_end)
    {
        // The last offset for each chunk points to one past the end of the local chunk
        return (*std::max_element(
            offsets_end.cend() - NODELETS(),
            offsets_end.cend()
        ));
    }

public:
    typedef T value_type;

    // Construct a ragged array from a list of bucket sizes
    static ragged_array
    from_sizes(const striped_array<long> & sizes)
    {
        auto offsets = compute_offsets(sizes);
        auto end_offsets = compute_end_offsets(offsets);
        long num_items = longest_chunk(offsets) * NODELETS();
        return ragged_array(num_items, std::move(offsets), std::move(end_offsets));
    }

    class subarray {
    private:
        T* first;
        T* last;
    public:
        subarray(T * first, T * last) : first(first), last(last) {}

        T& operator[] (long i) { return *(first + i * NODELETS()); }

        class iterator {
        public:
            typedef iterator self_type;
            typedef std::forward_iterator_tag iterator_category;
            typedef T value_type;
            typedef ptrdiff_t difference_type;
            typedef T* pointer;
            typedef T& reference;
            iterator(T * pos) : pos(pos) {}
            self_type operator++() { pos += NODELETS(); return *this; }
            reference operator*() { return *pos; }
            pointer operator->() { return pos; }

            bool operator==(const self_type& rhs) { return pos == rhs.pos; }
            bool operator!=(const self_type& rhs) { return pos != rhs.pos; }
        private:
            pointer pos;
        };

        iterator begin() { return iterator(first); }
        iterator end() { return iterator(last); }
        long size() { return (last - first) / NODELETS(); }
    };

    subarray
    operator[] (long i)
    {
        long slot = i % NODELETS();
        T* first = &items[offsets_begin[i] * NODELETS() + slot];
        T* last = &items[offsets_end[i] * NODELETS() + slot];
        return subarray(first, last);
    }
    // TODO const index operator

    class iterator {
    public:
        typedef iterator self_type;
        typedef std::random_access_iterator_tag iterator_category;
        typedef subarray value_type;
        typedef long difference_type;
        typedef long pointer;
        typedef subarray reference;
        iterator(ragged_array& array, long pos) : array(array), pos(pos) {}
        self_type operator++() { ++pos; return *this; }
        reference operator*() { return array[pos]; }
        pointer operator->() { return pos; }
        bool operator==(const self_type& rhs) { return pos == rhs.pos; }
        bool operator!=(const self_type& rhs) { return pos != rhs.pos; }
    private:
        ragged_array& array;
        pointer pos;
    };

    iterator begin() { return iterator(*this, 0); }
    iterator end() { return iterator(*this, offsets_begin.size()-1); }

    // Debug, print out internal structures to stdout
    void dump() {
        printf("%li items\n", items.size());
        printf("Offsets: \n");
        for (long i = 0; i < offsets_begin.size(); ++i) {
            printf("%li: %li-%li\n", i, offsets_begin[i], offsets_end[i]);
        }
    }
};

} // end namespace emu