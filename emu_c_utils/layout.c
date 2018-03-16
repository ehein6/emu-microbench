/*! \file layout
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu layout debugging
*/
#include "layout.h"

#include <assert.h>
#include <stdio.h>

#if defined(__le64__)

#include <memoryweb.h>

uint64_t grab_bits(uint64_t x, size_t begin, size_t end)
{
    assert(begin > end);
    assert(begin < 64 && end < 64);

    uint64_t mask = ~0;           // start with all ones
    mask >>= 63 - (begin - end);  // truncate to desired size
    mask <<= end;                 // move to desired location
    x &= mask;                    // mask off undesired bits
    x >>= end;                    // shift bits down to get number
    return x;
}

int ctz(unsigned long x)
{
    int n = 0;
    while((x & 1) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
}

struct emu_pointer
examine_emu_pointer(void * p)
{
    uint64_t ptr = (uint64_t)p;

    int m = ctz(BYTES_PER_NODELET());

    struct emu_pointer layout;
    layout.view =           grab_bits(ptr, 63, 56);
    layout.node_id =        grab_bits(ptr, 54, m + 3);
    layout.nodelet_id =     grab_bits(ptr, m + 2, m);
    layout.nodelet_addr =   grab_bits(ptr, m - 1, 3);
    layout.byte_offset =    grab_bits(ptr, 2, 0);
    return layout;
}

void print_emu_pointer(void * ptr)
{
    struct emu_pointer layout = examine_emu_pointer(ptr);
    fprintf(stdout, "V%lli:%lli:%lli:%x[%lli]",
        layout.view,
        layout.node_id,
        layout.nodelet_id,
        layout.nodelet_addr,
        layout.byte_offset
    );
    fflush(stdout);
}

/*
TEST:
print_emu_pointer(
    0b0000000100000000000000000001001000000000000000000000000000001001
);
Should print V1:1:1:1[1] when m = 33
*/

int pointers_are_on_same_nodelet(void * a, void *b)
{
    struct emu_pointer ptr_a = examine_emu_pointer(a);
    struct emu_pointer ptr_b = examine_emu_pointer(b);
    return ptr_a.node_id == ptr_b.node_id && ptr_a.nodelet_id == ptr_b.nodelet_id;
}

// These functions don't really do anything on a non-emu system, but we still want it to compile
#else

struct emu_pointer
examine_emu_pointer(void * ptr)
{
    struct emu_pointer p = {0};
    p.view = 1;
    p.byte_offset = ((uint64_t)ptr) & 0x3;
    p.nodelet_addr = (uint64_t)ptr>>3;
    return p;
}

void print_emu_pointer(void * ptr)
{
    fprintf(stderr, "%p", ptr);
}

int pointers_are_on_same_nodelet(void * a, void *b)
{
    return 1;
}

#endif
