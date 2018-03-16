/*! \file emu_scatter_gather
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu scatter/gather
 */
#pragma once

#include "emu_chunked_array.h"

// Copies a chunked array to a local array
// Number of items and size per item MUST match between the two arrays
void
emu_chunked_array_to_local(emu_chunked_array * self, void * local_array);

// Copies a local array to a chunked array
// Number of items and size per item MUST match between the two arrays
void
emu_chunked_array_from_local(emu_chunked_array * self, void * local_array);
