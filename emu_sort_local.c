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

/* Bitonic sort functions */
static void p_bitonic_sort(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain);
static void p_bitonic_merge(void *base, size_t low, size_t num, int (*compar)(const void *, const void *), bool asec, size_t size, size_t grain);

/* Merge sort functions */
static void p_merge_sort(void *base, void *temp, size_t nelem, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t grain);
static void p_merge(void *base, void *temp, size_t size, int (*compar)(const void *, const void *), size_t l, size_t r, size_t m);

/* Quick sort functions */
static void p_quick_sort(void *l, void *r, size_t size, int (*compar)(const void *, const void *), size_t grain);
//static void p_partition(void);

/* Parallel merge -- merge sort */
static void pss_base(void *xs, void *xe, void *z, size_t size, int (*compar)(const void *, const void *));
static void *upper_bound(void *xs, void *xe, size_t size, int (*compar)(const void *, const void *), void *x);
static void *lower_bound(void *xs, void *xe, size_t size, int (*compar)(const void *, const void *), void *x);
static void pss_serial_merge(void *xs, void *xe, void *ys, void *ye, void *z, size_t size, int (*compar)(const void *, const void *));
static void pss_merge(void *xs, void *xe, void *ys, void *ye, void *z, size_t size, int (*compar)(const void *, const void *));
static void pss_helper(void *xs, void *xe, void *z, size_t size, bool inplace, int (*compar)(const void *, const void *), size_t grain);

/* Common utilities */
static void insertion_sort(void *base, size_t nelem, size_t size, int (*compar)(const void *, const void *));
static inline void swap(void *a, void *b, size_t size);
static inline void emu_memcpy(void *dest, const void *src, size_t len);

/* Helper to print array */
static void
print_array(long *arr, size_t nelem)
{
    printf("Size : %lu -- ", nelem);
    for (size_t i = 0; i < nelem; i++) {
        printf("%ld ", arr[i]);
    }
    printf("\n");
}

/* Constants for parallel merge sort */

#define P_MERGE_SIZE_HIGH 128
#define P_MERGE_FACTOR_HIGH 6
#define P_MERGE_FACTOR_LOW 3
#define P_MERGE_INSERTION_COND 32

/* Constants for the parallel bitonic-qsort hybrid */
#define MIN_BITONIC_LENGTH 32
#define BITONIC_GRAIN(n) n >> 5

/* Constants for the parallel quick sort */
#define P_QUICK_FACTOR 3
#define P_QUICK_SORT_GRAIN(n) n >> P_QUICK_FACTOR

#define SWAP(a, b, size)                    \
    do                                      \
        {                                   \
            size_t __size = (size);         \
            char *__a = (a), *__b = (b);    \
            do                              \
                {                           \
                    char __tmp = *__a;      \
                    *__a++ = *__b;          \
                    *__b++ = __tmp;         \
                } while (--__size > 0);     \
        } while (0)

/*
 * emu_sort_local -- Call the best sort based on input parameters
 */
void
emu_sort_local(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    if (num >= MIN_BITONIC_LENGTH) {

        size_t grain;
        if (num > P_MERGE_SIZE_HIGH) {
            grain = num >> P_MERGE_FACTOR_HIGH;
        } else {
            grain = num >> P_MERGE_FACTOR_LOW;
        }
        char *temp = malloc(size * num);
        p_merge_sort(base, (void *) temp, num, size, compar, 0, num - 1, grain);
        free(temp);

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
 * Specific call to the parallel merge sort function
 */
void
emu_sort_local_merge(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    if (num > 1) {
        /* Allocate temporary array -- on failure call the inplace quick sort */
        char *temp = malloc(size * num);
        if (!temp) {
            emu_sort_local_quick(base, num, size, compar);
            return;
        }

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

/*
 * Specific call to the parallel quick sort function
 */
void
emu_sort_local_quick(void *base, size_t num, size_t size, int (*compar)(const void *, const void *))
{
    p_quick_sort(base, base + size * (num - 1), size, compar, (size_t) P_QUICK_SORT_GRAIN(num));
}

/*
 * Function to peform a parallel merge sort with parallel merge on an input array, has the same signature
 * as generic stdlib qsort
 */
void
emu_sort_local_pss(void *base, size_t nelem, size_t size, int (*compar)(const void *, const void *))
{
    /* Allocate buffer -- same size as the array that needs to be sorted */
    void *z = malloc(size * nelem);
    if (!z) {
        /* If could not allocate temporary buffer call the quick sort function */
        qsort(base, nelem, size, compar);
        return;
    }
    pss_helper(base, base + size * nelem, z, size, true, compar, nelem << 3);
}


/******* Internal functions -- All Static *******/

/****** Parallel Merge Sort Functions (Old version) ******/

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
            p_merge_sort(base, temp, (r - m), size, compar, m + 1, r, grain);

            cilk_sync;
            p_merge(base, temp, size, compar, l, r, m);
        } else if (nelem > 1) {
            if (nelem <= P_MERGE_INSERTION_COND) {
                /* try insertion sort for arrays smaller than 32 */
                insertion_sort(base + l * size, nelem, size, compar);
            } else { /* For anything bigger than that, keep on recursing down */
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

    /* copy into temp buffer */
    for (size_t i = 0; i < n1; i++) {
        emu_memcpy(temp + (i + l) * size, base + (i + l) * size, size);
    }

    for (size_t i = 0; i < n2; i++) {
        emu_memcpy(temp + (i + (m + 1)) * size, base + (i + (m + 1)) * size, size);
    }

    size_t k = l;
    size_t i = l;
    size_t j = m + 1;

    /* merge step */
    while (i < (n1 + l) && j < (n2 + m + 1)) {
        if (compar(temp + i * size, temp + j * size) > 0) {
            emu_memcpy(base + k * size, temp + j * size, size);
            j++;
        } else {
            emu_memcpy(base + k * size, temp + i * size, size);
            i++;
        }
        k++;
    }

    /* copy over the remaining elements of the left half */
    while (i < (n1 + l)) {
        emu_memcpy(base + k * size, temp + i * size, size);
        i++;
        k++;
    }

    /* copy over the remaining elements of the right half */
    while (j < (n2 + m + 1)) {
        emu_memcpy(base + k * size, temp + j * size, size);
        j++;
        k++;
    }
}

/****** Parallel Bitonic Sort Functions ******/

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
    /* spawn new threads for anything larger than grain size */
    if (num > grain) {
        size_t m = num / 2;
        cilk_spawn p_bitonic_sort(base, low, m, compar, !asec, size, grain);
        p_bitonic_sort(base, low+m, num - m, compar, asec, size, grain);

        cilk_sync;
        p_bitonic_merge(base, low, num, compar, asec, size, grain); /* merge the sorted portions */
    } else if (num > 1) {

        qsort(base + size * low, num, size, compar);
        /* Right now we are just reversing the sorted array -- hacky */
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
merge_worker(long begin, long end, va_list args)
{
    int (*compar)(const void *, const void *) = va_arg(args, int (*)(const void *, const void *));
    void *base = va_arg(args, void*);
    bool asec = (bool)va_arg(args, int); // bool must be promoted to int to pass through varargs
    size_t m = va_arg(args, size_t);
    size_t size = va_arg(args, size_t);

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
        emu_local_for(low, low+(num - m), grain,
            merge_worker, compar, base, (void *) asec, (void *) m, (void *) size);

        cilk_spawn p_bitonic_merge(base, low, m, compar, asec, size, grain);
        p_bitonic_merge(base, low + m, num - m, compar, asec, size, grain);
    } else if (num > 1) {
        /* sort the combined numbers in the direction of asec instead of calling the recursive implementation */
        qsort(base + low * size, num, size, compar);
        /* Right now we are just reversing the sorted array -- hacky */
        if (!asec) {
            for (size_t i = 0, j = num - 1; i < j; i++, j--) {
                char *left = ((char *) base) + (i + low) * size;
                char *right = ((char *) base) + (j + low) * size;
                swap(left, right, size);
            }
        }
    }
}

/****** Parallel Quick Sort Functions ******/

static void
p_quick_sort(void *l, void *r, size_t size, int (*compar)(const void *, const void *), size_t grain)
{
    size_t max_thresh = size * 4;
    size_t nelem = ((r - l) / size);

    char pivot[size];
    char *mid = l + (nelem >> 1) * size;

    if (compar((void *) mid, l) < 0) {
        swap(l, mid, size);
    }

    if (compar(r, (void *) mid) < 0) {
        swap(mid, r, size);
        if (compar((void *) mid, l) < 0) {
            swap(mid, l, size);
        }
    }

    emu_memcpy(pivot, mid, size); /* Make a copy of the pivot */

    char *lo = l + size;
    char *hi = r - size;
    while (lo <= hi) {
        while (compar((void *) lo, (void *) pivot) < 0) {
            lo += size;
        }

        while (compar((void *) hi, (void *) pivot) > 0) {
            hi -= size;
        }

        if (lo <= hi) {
            /* Swap if not the same location */
            if (lo < hi) {
                swap(lo, hi, size);
            }
            lo += size;
            hi -= size;
        }
    }

    if (nelem > grain) {
        if (l < (void *) hi) {
            cilk_spawn p_quick_sort(l, hi, size, compar, grain);
        }

        if ((void *) lo < r) {
            cilk_spawn p_quick_sort(lo, r, size, compar, grain);
        }

        cilk_sync;
    } else {
        if (l < (void *) hi) {
            p_quick_sort(l, hi, size, compar, grain);
        }

        if ((void *) lo < r) {
            p_quick_sort(lo, r, size, compar, grain);
        }
    }
}

static void p_partition(void);

/****** Parallel Merge Sort Functions ******/

/* TODO: Based on size of z array, serial sort should be either quick sort or it should be insertion sort */

/* sorts the array [xs, xe) and puts the result in z */
static void
pss_base(void *xs, void *xe, void *z, size_t size, int (*compar)(const void *, const void *))
{
    /* For now, call qsort and put it in z */
    size_t nelem = ((size_t) (xe - xs)) / size;
    emu_sort_local_quick(xs, nelem, size, compar);
    emu_memcpy(z, xs, size * nelem);
}

/* Returns the first element that is strictly greater than x in the array between [xs, xe) */
static void *
upper_bound(void *xs, void *xe, size_t size, int (*compar)(const void *, const void *), void *x)
{
    void *xm;
    size_t xn;
    while (xs < xe) {
        xn = ((size_t) (xe - xs)) / size;
        xm = xs + (xn / 2) * size;
        if (compar (x, xm) >= 0) {
            xs = xm + size; /* move left pointer */
        } else {
            xe = xm;
        }
    }
    return xs;
}

/* Returns first element that is not less than x in the array between [xs, xe) */
static void *
lower_bound(void *xs, void *xe, size_t size, int (*compar)(const void *, const void *), void *x)
{
    void *xm;
    size_t xn;
    while (xs < xe) {
        xn = ((size_t) (xe - xs)) / size;
        xm = xs + (xn / 2) * size;
        if (compar (x, xm) <= 0) {
            xe = xm;
        } else {
            xs = xm + size;
        }
    }
    return xs;
}

static void
pss_serial_merge(void *xs, void *xe, void *ys, void *ye, void *z, size_t size, int (*compar)(const void *, const void *))
{
    /* Merge both the halves */
    while (xs != xe && ys != ye) {
        if (compar (xs, ys) < 0) {
            emu_memcpy(z, xs, size);
            xs += size;
        } else {
            emu_memcpy(z, ys, size);
            ys += size;
        }
        z += size;
    }

    /* Remaining elements of left half */
    while (xs != xe) {
        emu_memcpy(z, xs, size);
        xs += size;
        z += size;
    }

    /* Remaining elements of the right half */
    while (ys != ye) {
        emu_memcpy(z, ys, size);
        ys += size;
        z += size;
    }
}

static void
pss_merge(void *xs, void *xe, void *ys, void *ye, void *z, size_t size, int (*compar)(const void *, const void *))
{
    const size_t P_MERGE_CUTOFF = 2000;

    size_t nelem =  ((size_t) ((xe - xs) + (ye - ys))) / size;

    if (nelem <= P_MERGE_CUTOFF) { /* Perform a serial merge if we have exceeded number of threads */
        pss_serial_merge(xs, xe, ys, ye, z, size, compar);
    } else {
        void *xm;
        void *ym;

        if ( (xe - xs) < (ye - ys) ) {
            size_t yn =  ((size_t) (ye - ys)) / size;
            ym = ys + (yn / 2) * size;
            xm = upper_bound(xs, xe, size, compar, ym);
        } else {
            size_t xn = ((size_t) (xe - xs)) / size;
            xm = xs + (xn / 2) * size;
            ym = lower_bound(ys, ye, size, compar, xm);
        }

        cilk_spawn pss_merge(xs, xm, ys, ym, z, size, compar); /* merge the left half of the array */

        z += ((xm - xs) + (ym - ys)); /* increment z to correct size */

        pss_merge(xm, xe, ym, ye, z, size, compar); /* xs becomes xm and ys becomes ym */

        cilk_sync;

    }
}

static void
pss_helper(void *xs, void *xe, void *z, size_t size, bool inplace, int (*compar)(const void *, const void *), size_t grain)
{
    const size_t PSS_CUTOFF = 500;

    /* sequential sort should put the data in the z array */
    size_t nelem = ((size_t) (xe - xs)) / size;

    if ( nelem <= PSS_CUTOFF ) {
        /* sort the base array into z */
        pss_base(xs, xe, z, size, compar);
    } else {
        void *xm = xs + (nelem / 2) * size;
        void *zm = z + (xm - xs);
        void *ze = z + (xe - xs);

        cilk_spawn pss_helper(xs, xm, z, size, !inplace, compar, grain);
        pss_helper(xm, xe, zm, size, !inplace, compar, grain);

        cilk_sync;

        /* Either we merge into z or x depending on the recursion level */
        if (inplace) {
            pss_merge(z, zm, zm, ze, xs, size, compar);
        } else {
            pss_merge(xs, xm, xm, xe, z, size, compar);
        }

    }
}

/******* Utility Functions *******/

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
    char key[size];
    for (size_t i = 1; i < nelem; i++) {
        /* create the key */
        emu_memcpy(key, base + i * size, size);

        long j = i - 1;
        while (j >= 0 && compar(base + j * size, (void *) key) > 0) {
            emu_memcpy(base + (j + 1) * size, base + j * size, size);
            j = j -1;
        }
        /* put the base in its place */
        emu_memcpy(base + (j + 1) * size, key, size);
    }
}

/*
 * Function to swap data pointed to by two pointers
 *
 * @param a -- The first pointer
 * @param b -- The second pointer
 * @param size -- The size of the data to swap
 */
static inline void
swap(void *a, void *b, size_t size)
{
    if (!(size & (sizeof(long) - 1))) {
        long *p = a;
        long *q = b;
        long tmp;
        do {
            tmp = *p;
            *p++ = *q;
            *q++ = tmp;
        } while ( (size -= sizeof(long)) );
    }

    /* This code does not work on EMU -- investigate why? */

    // else {
    //     char *p = a, *q = b, t;
    //     for (size_t i = 0; i < size; i++) {
    //         t = p[i];
    //         p[i] = q[i];
    //         q[i] = t;
    //     }
    // }
}

static inline void
emu_memcpy(void *dest, const void *src, size_t len)
{
    /* If not a multiple of sizeof(long), just call the library function */
    if (!(len & (sizeof(long) - 1))) {
        long *d = dest;
        const long *s = src;
        while (len) {
            *d++ = *s++;
            len -= sizeof(long);
        }
    }
    // else {
    //     memcpy(dest, src, len);
    // }
}
