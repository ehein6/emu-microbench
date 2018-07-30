#include "common.h"
#include "spawn_templates.h"
#include "chunked_array.h"
#include "striped_array.h"
#include "ragged_array.h"
#include "mirrored.h"

extern "C" {
#ifdef __le64__
extern "C" {
#include <memoryweb.h>
}
#else
#include "memoryweb_x86.h"
#endif
}

using namespace emu;


int main(int argc, char** argv)
{
    striped_array<long> sizes {4, 4, 4, 4};

    printf("Alloc ragged array...\n");
    auto csr = ragged_array<long>::from_sizes(sizes);
    csr.dump();
    printf("Fill ragged array...\n");
    long val = 0;
    for (long row = 0; row < sizes.size(); ++row) {
        for (long col = 0; col < sizes[row]; ++col) {
            assert(csr[row].size() == sizes[row]);
            csr[row][col] = val++;
        }
    }

    LOG("Dump ragged array...\n");
    for (long row = 0; row < sizes.size(); ++row) {
        for (long col = 0; col < sizes[row]; ++col) {
            LOG("csr[%li][%li] = %li\n", row, col, csr[row][col]);
        }
    }

    LOG("Test iterator access...\n");
    for (auto rowiter = csr.begin(); rowiter != csr.end(); ++rowiter) {
        auto row = *rowiter;
        for (auto coliter = row.begin(); coliter != row.end(); ++coliter) {
            LOG("%li, ", *coliter);
        }
    }
    LOG("\n");

    LOG("Test range-based for...\n");
    for (auto row : csr) {
        for (long col : row) {
            LOG("%li, ", col);
        }
    }
    LOG("\n");

    return 0;
}
