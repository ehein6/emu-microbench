#pragma once

void
emu_chunked_array_apply_v0(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end)
);

void
emu_chunked_array_apply_v1(
    void ** array, long num_elements, long grain,
    void * arg1,
    void (*worker)(long begin, long end, void * arg1)
);

void
emu_chunked_array_apply_v2(
    void ** array, long num_elements, long grain,
    void * arg1, void * arg2,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
);
