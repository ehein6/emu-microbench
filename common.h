#pragma once

// Logging macro. Flush right away since Emu hardware usually doesn't
// HACK disable logging
#define LOG(...)

// Assert with custom error message
static inline void
runtime_assert(bool condition, const char* message) {
    if (!condition) {
        LOG("ERROR: %s\n", message);
        exit(1);
    }
}