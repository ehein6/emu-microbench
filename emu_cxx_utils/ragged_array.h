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
    , offsets_begin(offsets_begin)
    , offsets_end(offsets_end)
    {
    }

    static long
    compute_chunk_size(/* TODO const */ striped_array<long>& offsets)
    {
        striped_array<long> elements_per_nodelet(NODELETS());
        offsets.parallel_apply([&](long i) {
            long nodelet = i % NODELETS();
            // TODO fix nasty access pattern here
            long size = offsets[i + 1] - offsets[i];
            REMOTE_ADD(&elements_per_nodelet[nodelet], size);
        });
        return *std::max_element(elements_per_nodelet.begin(), elements_per_nodelet.end());
    }

    // Helper function to compute list of offsets from array of bucket sizes
    static striped_array<long>
    compute_offsets(const striped_array<long> & sizes)
    {
        // Make another array with the same length
        // TODO should be replicated
        striped_array<long> offsets(sizes.size() + 1);

        // TODO need prefix scan here
        long cum_sum = 0;
        for (long i = 0; i < sizes.size(); ++i) {
            offsets[i] = cum_sum;
            cum_sum += sizes[i];
        }
        offsets[sizes.size()] = cum_sum;
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

public:
    typedef T value_type;

    // Construct a ragged array from a list of bucket sizes
    static ragged_array
    from_sizes(const striped_array<long> & sizes)
    {
        auto offsets = compute_offsets(sizes);
        long num_items = compute_chunk_size(offsets) * NODELETS();
        auto end_offsets = compute_end_offsets(offsets);
        return ragged_array(num_items, std::move(offsets), std::move(end_offsets));
    }

    // Construct a ragged array from a list of offsets
    static ragged_array
    from_offsets(striped_array<long> offsets)
    {
        long num_items = compute_chunk_size(offsets) * NODELETS();
        auto end_offsets = compute_end_offsets(offsets);
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

};

} // end namespace emu