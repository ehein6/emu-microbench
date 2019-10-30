.text
.global   @lock_cas_mutex_D
@lock_cas_mutex_D:
    .function_begin
    eta     2               // A = E2 (pointer to lock var)
    lsr     1               // D = 1
    dte     2               // E2 = 1
%spin:
    lsr     0               // D = 0
    cas     2               // if (*A==0) *A=1
    bnz     %spin           // if cas failed goto spin
    jmpe    1               // return
    .function_end

.global   @lock_cas_mutex_E
@lock_cas_mutex_E:
    .function_begin
    eta     2               // A = E2 (pointer to lock var)
    lsr     1               // D = 1
    dte     2               // E2 = 1
%spin:
    ld                      // D = *A
    bdnz    %spin           // if (D) goto spin
    cas     2               // if (*A==0) *A=1
    bnz     %spin           // if cas failed goto spin
    jmpe    1               // return
    .function_end

.end

