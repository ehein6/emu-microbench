#pragma once

// Logging macro. Flush right away since Emu hardware usually doesn't
#define LOG(...) fprintf(stdout, __VA_ARGS__); fflush(stdout);

// Assert with custom error message
static inline void
runtime_assert(bool condition, const char* message) {
    if (!condition) {
        LOG("ERROR: %s\n", message);
        exit(1);
    }
}