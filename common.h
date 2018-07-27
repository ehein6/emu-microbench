#pragma once

#include <stdio.h>
#include <stdlib.h>


// Logging macro. Flush right away since Emu hardware usually doesn't
#define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);

// Assert with custom error message
static inline void
runtime_assert(bool condition, const char* message) {
    if (!condition) {
        LOG("ERROR: %s\n", message);
        exit(1);
    }
}