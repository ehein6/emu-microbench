/*! \file emu_c_utils.h
 \date March 15, 2018
 \author Eric Hein 
 \brief Header file for Emu c utilities
 */
#pragma once

#include <stddef.h>

#define REPLICATE(X) replicate_struct((X), sizeof(*(X)))
void replicate_struct(void * s, size_t sz);
