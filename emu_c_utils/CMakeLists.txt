add_library(emu_c_utils
    memoryweb_x86.h
    emu_grain_helpers.h emu_grain_helpers.c
    emu_for_1d.c emu_for_1d.h
    emu_reduce_1d.c emu_reduce_1d.h
    emu_chunked_array.h emu_chunked_array.c emu_for_2d.c emu_reduce_2d.c
    emu_scatter_gather.c emu_scatter_gather.h
    emu_for_local.h emu_for_local.c
    emu_sort_local.h emu_sort_local.c
    emu_c_utils.h emu_c_utils.c
    hooks.h hooks.c
    layout.c layout.h
)