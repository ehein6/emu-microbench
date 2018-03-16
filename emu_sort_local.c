/*! \file emu_sort_local
 \date Feb. 21, 2018
 \author Anirudh Jain 
 \author Eric Hein 
 \brief Source file for Emu chunked sort_local
 */

/**
 * Author:
 *   Anirudh Jain <anirudh.j@gatech.edu> Dec 12, 2017
 * Modified:
 *   Eric Hein <ehein6@gatech.edu> Feb 21, 2018
 */

#include "emu_sort_local.h"
#include <cilk/cilk.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "emu_grain_helpers.h"
#include "emu_for_local.h"

// Bitonic sort functions
static void p_bitonic_sort(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain);
static void p_bitonic_merge(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain);

// Merge sort functions
static void p_merge_sort(void *base, void *temp, size_t nelem, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t grain);
static void p_merge(void *base, void *temp, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t m);

// Common utilities
static void insertion_sort(void *base, size_t nelem, size_t size, int (*compar)(const void *, const void *));
static void swap(void *a, void *b, size_t size);


// Helper to print array
//static void
//print_array(long *arr, size_t nelem)
//{
//    printf("Size : %lu -- ", nelem);
//    for (size_t i = 0; i < nelem; i++) {
//        printf("%ld ", arr[i]);
//    }
//    printf("\n");
//}

// Constants for parallel merge sort

#define P_MERGE_SIZE_HIGH 128
#define P_MERGE_FACTOR_HIGH 5
#define P_MERGE_FACTOR_LOW 3


#define MIN_BITONIC_LENGTH 32
#define BITONIC_GRAIN(n) n >> 5

/*
 * emu_sort_local -- Call the best sort based on input parameters
 */
void
emu_sort_local(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    if (num >= MIN_BITONIC_LENGTH) {
        // p_bitonic_sort(base, 0, num, compar, true, size, 4);

        size_t grain;
        if (num > P_MERGE_SIZE_HIGH) {
            grain = num >> P_MERGE_FACTOR_HIGH;
        } else {
            grain = num >> P_MERGE_FACTOR_LOW;
        }
        char *temp = malloc(size * num);
        p_merge_sort(base, (void *) temp, num, size, compar, 0, num - 1, grain);
        free(temp);

        //insertion_sort(base, num, size, compar);

    } else {
        qsort(base, num, size, compar);
    }
}

/*
 * Specific call to the parallel bitonic sort function
 */
void
emu_sort_local_bitonic(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    if (num >= MIN_BITONIC_LENGTH) {
        p_bitonic_sort(base, 0, num, compar, true, size, (size_t) BITONIC_GRAIN(num));
    } else {
        qsort(base, num, size, compar);
    }
}

/*
 * Specific call to the parallel merge function
 */
void
emu_sort_local_merge(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    // Call only if more than one element
    if (num > 1) {
        char *temp = malloc(size * num);
        size_t grain;
        if (num > P_MERGE_SIZE_HIGH) {
            grain = num >> P_MERGE_FACTOR_HIGH;
        } else {
            grain = num >> P_MERGE_FACTOR_LOW;
        }
        p_merge_sort(base, (void *) temp, num, size, compar, 0, num - 1, grain);
        free(temp);
    }
}

/* Internal functions -- All Static */

/**
 * Insertion sort implementation for smaller array sizes.
 *
 * @param base -- The array
 * @param nelem -- The array size
 * @param size -- Size of each element of the array
 * @param compar -- The comparator
 *
 */
static void
insertion_sort(void *base, size_t nelem, size_t size, int (*compar)(const void *, const void *))
{
    char *key = malloc(size);
    for (size_t i = 1; i < nelem; i++) {
        // create the key

        memcpy(key, base + i * size, size);

        long j = i - 1;
        while (j >= 0 && compar(base + j * size, (void *) key) > 0) {
            memcpy(base + (j + 1) * size, base + j * size, size);
            j = j -1;
        }
        // put the base in its place
        memcpy(base + (j + 1) * size, key, size);
    }
    free(key);
    // print_array(base, nelem);
}

/**
 * Merge sort implementation using the cilk threading library
 *
 * @param base -- The array
 * @param temp -- Temporary array serving as a buffer
 * @param nelem -- Number of elements in the array
 * @param size -- Size of each element in bytes
 * @param compar -- The comparator
 * @param l -- The leftmost element
 * @param r -- The rightmost element of the array
 * @param grain -- Grain size which decides if we spawn new threads
 */
static void
p_merge_sort(void *base, void *temp, size_t nelem, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t grain)
{
    if (r > l) {

        if (nelem > grain) {
            size_t m = (l + r) / 2;

            cilk_spawn p_merge_sort(base, temp, (m - l + 1), size, compar, l, m, grain);
            cilk_spawn p_merge_sort(base, temp, (r - m), size, compar, m + 1, r, grain);

            cilk_sync;
            p_merge(base, temp, size, compar, l, r, m);
        } else if (nelem > 1) {
            if (grain <= 32) {
                // try insertion sort for arrays smaller than 32
                insertion_sort(base + l * size, nelem, size, compar);
            } else { // For anything bigger than that, keep on recursing down
                size_t m = (l + r) / 2;

                p_merge_sort(base, temp, (m - l + 1), size, compar, l, m, grain);
                p_merge_sort(base, temp, (r - m), size, compar, m + 1, r, grain);

                p_merge(base, temp, size, compar, l, r, m);
            }
        }
    }
}

// TODO -- clean up this code for reducing temporaries and reducing arithmetic computations

/**
 * Merge utility for the merge sort implementation using the cilk threading library
 *
 * @param base -- The array
 * @param temp -- Temporary array serving as a buffer
 * @param size -- Size of each element in bytes
 * @param compar -- The comparator
 * @param l -- The leftmost element
 * @param r -- The rightmost element of the array
 * @param m -- The halfway point between the two sub-arrays
 */
static void
p_merge(void *base, void *temp, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t m)
{
    size_t n1 = m - l + 1;
    size_t n2 = r - m;

    // copy into temp buffer
    for (size_t i = 0; i < n1; i++) {
        memcpy(temp + (i + l) * size, base + (i + l) * size, size);
    }

    for (size_t i = 0; i < n2; i++) {
        memcpy(temp + (i + (m + 1)) * size, base + (i + (m + 1)) * size, size);
    }

    size_t k = l;
    size_t i = l;
    size_t j = m + 1;

    // merge step
    while (i < (n1 + l) && j < (n2 + m + 1)) {
        if (compar(temp + i * size, temp + j * size) > 0) {
            memcpy(base + k * size, temp + j * size, size);
            j++;
        } else {
            memcpy(base + k * size, temp + i * size, size);
            i++;
        }
        k++;
    }

    // copy over the remaining elements of the left half
    while (i < (n1 + l)) {
        memcpy(base + k * size, temp + i * size, size);
        i++;
        k++;
    }

    // copy over the remaining elements of the right half
    while (j < (n2 + m + 1)) {
        memcpy(base + k * size, temp + j * size, size);
        j++;
        k++;
    }
}

/*
 * Bitonic sort function
 *
 * @param base Array pointer
 * @param low lower index
 * @param num number of elements to sort
 * @param compar comparator
 * @param asec True for soring in compar order
 * @param size size of each element of array in bytes
 */
static void
p_bitonic_sort(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain)
{
    // spawn new threads for anything larger than grain size
    if (num > grain) {
        size_t m = num / 2;
        cilk_spawn p_bitonic_sort(base, low, m, compar, asec, size, grain);
        cilk_spawn p_bitonic_sort(base, low+m, num - m, compar, !asec, size, grain);

        cilk_sync;
        p_bitonic_merge(base, low, num, compar, asec, size, grain); // merge the sorted portions
    } else if (num > 1) {

        qsort(base + size * low, num, size, compar);
        // Right now we are just reversing the sorted array -- hacky
        // compare against a standalone quicksort implementation
        if (!asec) {
            for (size_t i = 0, j = num - 1; i < j; i++, j--) {
                char *left = ((char *) base) + (i + low) * size;
                char *right = ((char *) base) + (j + low) * size;
                swap(left, right, size);
            }
        }
    }
}

/*
 * Helper function to get highest power of two less than a number
 */
static size_t
highestPowerofTwoLessThan(size_t n)
{
    size_t res = 1;
    while (res > 0 && res < n) {
        res <<= 1;
    }
    return res >> 1;
}

/*
 * Merge worker for the hybrid bitonic sort to be used by the cilk tree loop
 * @param begin Start range for the loop
 * @param end End range for the loop
 * @param arg1 comparator
 * @param arg2 base -- The array
 * @param arg3 asec -- Boolean for the order
 * @param arg4 m -- The divider between the 2 halves
 * @param arg5 size -- Size of each element
 */
static void
merge_worker(long begin, long end, void *arg1, void *arg2, void *arg3, void *arg4, void *arg5)
{
    int (*compar)(const void *, const void *) = arg1;
    void *base = arg2;
    bool asec = (bool) arg3;
    size_t m = (size_t) arg4;
    size_t size = (size_t) arg5;

    for (long i = begin; i < end; i++) {
        char *left = ((char *) base) + i * size;
        char *right = ((char *) base) + (i + m) * size;
        if (asec) {
            if (compar(left, right) >  0) {
                swap(left, right, size);
            }
        } else {
            if (compar(left, right) < 0) {
                swap(left, right, size);
            }
        }
    }
}

/*
 * Merge function of bitonic-qsort hybrid. Based on size of array either recurses down or calls qsort
 *
 * @param base -- The array
 * @param low -- The first element this call has to sort
 * @param num -- The number of elements in the array
 * @param compar -- The comparator
 * @param asec -- If true, sort in ascending order
 * @param size -- Size of each element of the array
 * @param grain -- Decide if parallelism is required
 *
 */
static void
p_bitonic_merge(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain)
{
    if (num > grain) {
        size_t m = highestPowerofTwoLessThan(num);

        // calling the spawn based for loop
        emu_local_for_v5(low, low+(num - m), grain,
            merge_worker, compar, base, (void *) asec, (void *) m, (void *) size);

        cilk_spawn p_bitonic_merge(base, low, m, compar, asec, size, grain);
        cilk_spawn p_bitonic_merge(base, low + m, num - m, compar, asec, size, grain);
    } else if (num > 1) {
        // sort the combined numbers in the direction of asec instead of calling
        // the recursive implementation
        qsort(base + low * size, num, size, compar);
        // Right now we are just reversing the sorted array -- hacky
        // compare against a standalone quicksort implementation
        if (!asec) {
            for (size_t i = 0, j = num - 1; i < j; i++, j--) {
                char *left = ((char *) base) + (i + low) * size;
                char *right = ((char *) base) + (j + low) * size;
                swap(left, right, size);
            }
        }
    }
}

/*
 * Function to swap data pointed to by two pointers
 *
 * @param a -- The first pointer
 * @param b -- The second pointer
 * @param size -- The size of the data to swap
 */
static void
swap(void *a, void *b, size_t size)
{
    if (!(size % sizeof(long))) {
        long *p =a, *q = b, t;
        for (size_t i = 0; i < size; i += sizeof(long)) {
            t = p[i];
            p[i] = q[i];
            q[i] = t;
        }
    }
    // else {
    //     char *p = a, *q = b, t;
    //     for (size_t i = 0; i < size; i++) {
    //         t = p[i];
    //         p[i] = q[i];
    //         q[i] = t;
    //     }
    // }
}
