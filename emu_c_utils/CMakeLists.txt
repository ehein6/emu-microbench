set(sources
#    emu_grain_helpers.c
#    emu_for_1d.c
#    emu_reduce_1d.c
#    emu_chunked_array.c
#    emu_for_2d.c
#    emu_reduce_2d.c
#    emu_scatter_gather.c
#    emu_for_local.c
#    emu_sort_local.c
    hooks.c
    layout.c
)

set(headers
    memoryweb_x86.h
    emu_grain_helpers.h
    emu_for_1d.h
    emu_reduce_1d.h
    emu_chunked_array.h
    emu_scatter_gather.h
    emu_for_local.h
    emu_sort_local.h
    emu_c_utils.h
    hooks.h
    layout.h
)

add_library(emu_c_utils ${sources} ${headers})
set_target_properties(emu_c_utils PROPERTIES PUBLIC_HEADER "${headers}")
install(
    TARGETS                     emu_c_utils
    LIBRARY DESTINATION         lib
    ARCHIVE DESTINATION         lib
    PUBLIC_HEADER DESTINATION   include/emu_c_utils
)
