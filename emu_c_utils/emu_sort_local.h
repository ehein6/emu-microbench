/*! \file emu_sort_local
 \date Feb. 21, 2018
 \author Anirudh Jain 
 \author Eric Hein 
 \brief Header file for Emu chunked sort_local
 */

/**
 * Author:
 *   Anirudh Jain <anirudh.j@gatech.edu> Dec 12, 2017
 * Modified:
 *   Eric Hein <ehein6@gatech.edu> Feb 21, 2018
 */

#pragma once

#include <stddef.h>

void emu_sort_local(void *base, size_t num, size_t size, int (*compar)(const void *, const void *));

/* Specific functions */
void emu_sort_local_bitonic(void *base, size_t num, size_t size, int (*compar)(const void *, const void *));
void emu_sort_local_merge(void *base, size_t num, size_t size, int (*compar)(const void *, const void *));
