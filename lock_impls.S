.text

//extern "C" void lock_cas_mutex_D(volatile long * lock);
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

//extern "C" void lock_cas_mutex_E(volatile long * lock);
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

//template<> void worker(cas_mutex_D& mutex, volatile double * counter, long n);
.global @_Z6workerI11cas_mutex_DEvRT_PVdl
@_Z6workerI11cas_mutex_DEvRT_PVdl:
    .function_begin
    lsr     1               // D = 1
    dte     11              // E11 = 1 (used for CAS)
    eta     2               // A = &mutex
%loop:
    etd     4               // D = loop counter
    bnzd    %skip           // while(n--)
    jmpe    1               // return
%skip:
    dte     4               // E4 = loop counter
%spin:
    lsr     0               // D = 0
    cas     11              // if (*A==0) *A=1
    bnz     %spin           // if cas failed goto spin
%critical:
    eta     3               // A = &counter
    lit16   1023            // D = 0x3FF
    sllc    52              // D = 0x3FF0000000000000 (1.0)
    dte     10              // E10 = 1.0
    ld                      // load counter
    fpadde  10              // counter += 1.0
    std                     // store counter
%unlock:
    eta     2               // A = &mutex
    lsr     0               // D = 0
    std                     // unlock
    jmp     %loop           // end while
    .function_end

// template<> void worker(cas_mutex_E& mutex, volatile double * counter, long n)
.global @_Z6workerI11cas_mutex_EEvRT_PVdl
@_Z6workerI11cas_mutex_EEvRT_PVdl:
    .function_begin
    lsr     1               // D = 1
    dte     11              // E11 = 1 (used for CAS)
    eta     2               // A = &mutex
%loop:
    etd     4               // D = loop counter
    bnzd    %skip           // while(n--)
    jmpe    1               // return
%skip:
    dte     4               // E4 = loop counter
%spin:
    ld                      // D = *A
    bdnz    %spin           // if (D) goto spin
    cas     11              // if (*A==0) *A=1
    bnz     %spin           // if cas failed goto spin
%critical:
    eta     3               // A = &counter
    lit16   1023            // D = 0x3FF
    sllc    52              // D = 0x3FF0000000000000 (1.0)
    dte     10              // E10 = 1.0
    ld                      // load counter
    fpadde  10              // counter += 1.0
    std                     // store counter
%unlock:
    eta     2               // A = &mutex
    lsr     0               // D = 0
    std                     // unlock
    jmp     %loop           // end while
    .function_end

// template<> void worker(cas_mutex_F& mutex, volatile double * counter, long n)
.global @_Z6workerI11cas_mutex_FEvRT_PVdl
@_Z6workerI11cas_mutex_FEvRT_PVdl:
    .function_begin
    lsr     1               // D = 1
    dte     11              // E11 = 1 (used for CAS)
    eta     2               // A = &mutex
%loop:
    etd     4               // D = loop counter
    bnzd    %skip           // while(n--)
    jmpe    1               // return
%skip:
    dte     4               // E4 = loop counter
    jmp     %spin
%sleep:
    release 0xF             // RESCHEDULE
%spin:
    ld                      // D = *A
    bdnz    %sleep          // if (D) goto sleep
    cas     11              // if (*A==0) *A=1
    bnz     %sleep          // if cas failed goto sleep
%critical:
    eta     3               // A = &counter
    lit16   1023            // D = 0x3FF
    sllc    52              // D = 0x3FF0000000000000 (1.0)
    dte     10              // E10 = 1.0
    ld                      // load counter
    fpadde  10              // counter += 1.0
    std                     // store counter
%unlock:
    eta     2               // A = &mutex
    lsr     0               // D = 0
    std                     // unlock
    jmp     %loop           // end while
    .function_end

.end
