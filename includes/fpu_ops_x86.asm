; fpu_ops_x86.asm
; x86 MASM: integer FI* (16/32) + 64-bit emulation via FILD qword + *P,
; plus float/double F* and some preserve wrappers.
.686
.model flat, C
.code

; --- Minimal exports ---
PUBLIC FIADD16_from_ptr
PUBLIC FIADD32_from_ptr
PUBLIC FIADD64_from_ptr     ; implemented via FILD qword + FADDP

PUBLIC FISUB16_from_ptr
PUBLIC FISUB32_from_ptr
PUBLIC FISUB64_from_ptr     ; implemented via FILD qword + FSUBP

PUBLIC FISUBR16_from_ptr
PUBLIC FISUBR32_from_ptr
PUBLIC FISUBR64_from_ptr    ; implemented via FILD qword + FSUBRP

PUBLIC FIMUL16_from_ptr
PUBLIC FIMUL32_from_ptr
PUBLIC FIMUL64_from_ptr     ; implemented via FILD qword + FMULP

PUBLIC FIDIV16_from_ptr
PUBLIC FIDIV32_from_ptr
PUBLIC FIDIV64_from_ptr     ; implemented via FILD qword + FDIVP

PUBLIC FIDIVR16_from_ptr
PUBLIC FIDIVR32_from_ptr
PUBLIC FIDIVR64_from_ptr    ; implemented via FILD qword + FDIVRP

PUBLIC FICOMP16_from_ptr
PUBLIC FICOMP32_from_ptr
PUBLIC FICOMP64_from_ptr

PUBLIC FSIN_from_ptr
PUBLIC FCOS_from_ptr
PUBLIC FSINCOS_from_ptr

PUBLIC FPTAN_from_ptr
PUBLIC FPATAN_from_ptr

PUBLIC FPREM_from_ptr
PUBLIC FPREM1_from_ptr

PUBLIC FYL2X_from_ptr
PUBLIC FYL2XP1_from_ptr

PUBLIC FSCALE_from_ptr
PUBLIC FSQRT_from_ptr

; Float (32-bit)
PUBLIC FLD_f32_from_ptr
PUBLIC FADD_f32_from_ptr
PUBLIC FSUB_f32_from_ptr
PUBLIC FSUBR_f32_from_ptr
PUBLIC FMUL_f32_from_ptr
PUBLIC FDIV_f32_from_ptr
PUBLIC FDIVR_f32_from_ptr
PUBLIC FCOMP_f32_from_ptr

; Double (64-bit)
PUBLIC FLD_f64_from_ptr
PUBLIC FADD_f64_from_ptr
PUBLIC FSUB_f64_from_ptr
PUBLIC FSUBR_f64_from_ptr
PUBLIC FMUL_f64_from_ptr
PUBLIC FDIV_f64_from_ptr
PUBLIC FDIVR_f64_from_ptr
PUBLIC FCOMP_f64_from_ptr

; --- Preserving wrappers (32-bit examples) ---
PUBLIC preserve_FIADD32_from_ptr
PUBLIC preserve_FISUB32_from_ptr
PUBLIC preserve_FIMUL32_from_ptr
PUBLIC preserve_FIDIV32_from_ptr
PUBLIC preserve_FIDIVR32_from_ptr

PUBLIC preserve_FLD_f32_from_ptr
PUBLIC preserve_FADD_f32_from_ptr
PUBLIC preserve_FMUL_f32_from_ptr
PUBLIC preserve_FDIV_f32_from_ptr

; ========== Implementations ==========
; All minimal variants expect pointer argument at [esp+4]

; ----- FILD loaders for 16/32/64 (32-bit calling conv: pointer at [esp+4]) -----

PUBLIC FILD16_from_ptr
PUBLIC FILD32_from_ptr
PUBLIC FILD64_from_ptr

FILD16_from_ptr PROC
    mov eax, [esp+4]
    FILD WORD PTR [eax]
    ret
FILD16_from_ptr ENDP

FILD32_from_ptr PROC
    mov eax, [esp+4]
    FILD DWORD PTR [eax]
    ret
FILD32_from_ptr ENDP

FILD64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    ret
FILD64_from_ptr ENDP

; ----- FIADD (16/32) -----
FIADD16_from_ptr PROC
    mov eax, [esp+4]
    FIADD WORD PTR [eax]
    ret
FIADD16_from_ptr ENDP

FIADD32_from_ptr PROC
    mov eax, [esp+4]
    FIADD DWORD PTR [eax]
    ret
FIADD32_from_ptr ENDP

; ----- FIADD64 emulate via FILD qword + FADDP ST(1), ST(0) -----
FIADD64_from_ptr PROC
    mov eax, [esp+4]
    ; load 64-bit integer as float onto stack (ST0 = integer)
    FILD QWORD PTR [eax]
    ; add integer (ST0) to original value (now ST1); result in ST1, pop -> result in ST0
    FADDP ST(1), ST(0)
    ret
FIADD64_from_ptr ENDP

; ----- FISUB (16/32) -----
FISUB16_from_ptr PROC
    mov eax, [esp+4]
    FISUB WORD PTR [eax]
    ret
FISUB16_from_ptr ENDP

FISUB32_from_ptr PROC
    mov eax, [esp+4]
    FISUB DWORD PTR [eax]
    ret
FISUB32_from_ptr ENDP

; ----- FISUB64 emulate: original - integer -> FSUBP ST(1), ST(0) -----
FISUB64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    ; ST1 = original, ST0 = integer -> ST1 = ST1 - ST0, pop -> result in ST0
    FSUBP ST(1), ST(0)
    ret
FISUB64_from_ptr ENDP

; ----- FISUBR (16/32) -----
FISUBR16_from_ptr PROC
    mov eax, [esp+4]
    FISUBR WORD PTR [eax]
    ret
FISUBR16_from_ptr ENDP

FISUBR32_from_ptr PROC
    mov eax, [esp+4]
    FISUBR DWORD PTR [eax]
    ret
FISUBR32_from_ptr ENDP

; ----- FISUBR64 emulate: integer - original -> FSUBRP ST(1), ST(0) -----
FISUBR64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    ; FSUBRP ST(1),ST(0) computes ST1 = ST0 - ST1 then pop -> integer - original
    FSUBRP ST(1), ST(0)
    ret
FISUBR64_from_ptr ENDP

; ----- FIMUL (16/32) -----
FIMUL16_from_ptr PROC
    mov eax, [esp+4]
    FIMUL WORD PTR [eax]
    ret
FIMUL16_from_ptr ENDP

FIMUL32_from_ptr PROC
    mov eax, [esp+4]
    FIMUL DWORD PTR [eax]
    ret
FIMUL32_from_ptr ENDP

; ----- FIMUL64 emulate via FILD qword + FMULP ST(1), ST(0) -----
FIMUL64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    FMULP ST(1), ST(0)
    ret
FIMUL64_from_ptr ENDP

; ----- FICOMP (16/32/64) + FCOMP (float/double) -----

; 16-bit integer compare & pop
FICOMP16_from_ptr PROC
    mov eax, [esp+4]
    FICOMP WORD PTR [eax]
    ret
FICOMP16_from_ptr ENDP

; 32-bit integer compare & pop
FICOMP32_from_ptr PROC
    mov eax, [esp+4]
    FICOMP DWORD PTR [eax]
    ret
FICOMP32_from_ptr ENDP

; 64-bit integer compare & pop (emulated)
; Emulation steps:
;  - FILD QWORD PTR [eax]  ; push integer (Y)
;  - FXCH ST(1)            ; swap -> ST0 = original (X), ST1 = integer (Y)
;  - FCOMP ST(1)           ; compare X with Y and pop X (sets condition flags)
;  - FSTP ST(0)            ; pop the remaining integer (Y), restoring original stack minus X
FICOMP64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    FXCH ST(1)
    FCOMP ST(1)
    FSTP ST(0)
    ret
FICOMP64_from_ptr ENDP

; ----- FIDIV (16/32) -----
FIDIV16_from_ptr PROC
    mov eax, [esp+4]
    FIDIV WORD PTR [eax]
    ret
FIDIV16_from_ptr ENDP

FIDIV32_from_ptr PROC
    mov eax, [esp+4]
    FIDIV DWORD PTR [eax]
    ret
FIDIV32_from_ptr ENDP

; ----- FIDIV64 emulate via FILD qword + FDIVP ST(1), ST(0) (original / integer) -----
FIDIV64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    ; compute original / integer -> FDIVP ST(1), ST(0) (ST1 = ST1 / ST0, pop)
    FDIVP ST(1), ST(0)
    ret
FIDIV64_from_ptr ENDP

; ----- FIDIVR (16/32) -----
FIDIVR16_from_ptr PROC
    mov eax, [esp+4]
    FIDIVR WORD PTR [eax]
    ret
FIDIVR16_from_ptr ENDP

FIDIVR32_from_ptr PROC
    mov eax, [esp+4]
    FIDIVR DWORD PTR [eax]
    ret
FIDIVR32_from_ptr ENDP

; ----- FIDIVR64 emulate: integer / original -> FDIVRP ST(1), ST(0) -----
FIDIVR64_from_ptr PROC
    mov eax, [esp+4]
    FILD QWORD PTR [eax]
    ; FDIVRP ST(1),ST(0) computes ST1 = ST0 / ST1 (integer / original), pop
    FDIVRP ST(1), ST(0)
    ret
FIDIVR64_from_ptr ENDP

FSIN_from_ptr PROC
    mov eax, [esp+4]
    FSIN
    ret
FSIN_from_ptr ENDP

FCOS_from_ptr PROC
    mov eax, [esp+4]
    FCOS
    ret
FCOS_from_ptr ENDP

; computes sine(st0) -> st0, pushes cosine onto stack (st0 = cos pushed, st1 = sin)
FSINCOS_from_ptr PROC
    mov eax, [esp+4]
    FSINCOS
    ret
FSINCOS_from_ptr ENDP

; partial tangent: tangent(st0) -> st0, pushes 1.0 onto stack
FPTAN_from_ptr PROC
    mov eax, [esp+4]
    FPTAN
    ret
FPTAN_from_ptr ENDP

; partial arctangent: uses ST(1)=Y and ST(0)=X; result placed appropriately per x87 semantics
FPATAN_from_ptr PROC
    mov eax, [esp+4]
    FPATAN
    ret
FPATAN_from_ptr ENDP

; partial remainder instructions
FPREM_from_ptr PROC
    mov eax, [esp+4]
    FPREM
    ret
FPREM_from_ptr ENDP

FPREM1_from_ptr PROC
    mov eax, [esp+4]
    FPREM1
    ret
FPREM1_from_ptr ENDP

; logarithm helpers
FYL2X_from_ptr PROC
    mov eax, [esp+4]
    FYL2X
    ret
FYL2X_from_ptr ENDP

FYL2XP1_from_ptr PROC
    mov eax, [esp+4]
    FYL2XP1
    ret
FYL2XP1_from_ptr ENDP

; scale (uses ST(0) and ST(1))
FSCALE_from_ptr PROC
    mov eax, [esp+4]
    FSCALE
    ret
FSCALE_from_ptr ENDP

; sqrt
FSQRT_from_ptr PROC
    mov eax, [esp+4]
    FSQRT
    ret
FSQRT_from_ptr ENDP

; ----- float (32-bit) ops -----
FLD_f32_from_ptr PROC
    mov eax, [esp+4]
    FLD DWORD PTR [eax]
    ret
FLD_f32_from_ptr ENDP

FADD_f32_from_ptr PROC
    mov eax, [esp+4]
    FADD DWORD PTR [eax]
    ret
FADD_f32_from_ptr ENDP

FSUB_f32_from_ptr PROC
    mov eax, [esp+4]
    FSUB DWORD PTR [eax]
    ret
FSUB_f32_from_ptr ENDP

FSUBR_f32_from_ptr PROC
    mov eax, [esp+4]
    FSUBR DWORD PTR [eax]
    ret
FSUBR_f32_from_ptr ENDP

FMUL_f32_from_ptr PROC
    mov eax, [esp+4]
    FMUL DWORD PTR [eax]
    ret
FMUL_f32_from_ptr ENDP

FDIV_f32_from_ptr PROC
    mov eax, [esp+4]
    FDIV DWORD PTR [eax]
    ret
FDIV_f32_from_ptr ENDP

FDIVR_f32_from_ptr PROC
    mov eax, [esp+4]
    FDIVR DWORD PTR [eax]
    ret
FDIVR_f32_from_ptr ENDP

FCOMP_f32_from_ptr PROC
    mov eax, [esp+4]
    FCOMP DWORD PTR [eax]    ; compare ST0 with memory float and pop ST0
    ret
FCOMP_f32_from_ptr ENDP

; ----- double (64-bit) ops -----
FLD_f64_from_ptr PROC
    mov eax, [esp+4]
    FLD QWORD PTR [eax]
    ret
FLD_f64_from_ptr ENDP

FADD_f64_from_ptr PROC
    mov eax, [esp+4]
    FADD QWORD PTR [eax]
    ret
FADD_f64_from_ptr ENDP

FSUB_f64_from_ptr PROC
    mov eax, [esp+4]
    FSUB QWORD PTR [eax]
    ret
FSUB_f64_from_ptr ENDP

FSUBR_f64_from_ptr PROC
    mov eax, [esp+4]
    FSUBR QWORD PTR [eax]
    ret
FSUBR_f64_from_ptr ENDP

FMUL_f64_from_ptr PROC
    mov eax, [esp+4]
    FMUL QWORD PTR [eax]
    ret
FMUL_f64_from_ptr ENDP

FDIV_f64_from_ptr PROC
    mov eax, [esp+4]
    FDIV QWORD PTR [eax]
    ret
FDIV_f64_from_ptr ENDP

FDIVR_f64_from_ptr PROC
    mov eax, [esp+4]
    FDIVR QWORD PTR [eax]
    ret
FDIVR_f64_from_ptr ENDP

FCOMP_f64_from_ptr PROC
    mov eax, [esp+4]
    FCOMP QWORD PTR [eax]    ; compare ST0 with memory double and pop ST0
    ret
FCOMP_f64_from_ptr ENDP

; -----------------------
; Preserve wrappers (32-bit examples)
preserve_FIADD32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FIADD DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FIADD32_from_ptr ENDP

preserve_FISUB32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FISUB DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FISUB32_from_ptr ENDP

preserve_FIMUL32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FIMUL DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FIMUL32_from_ptr ENDP

preserve_FIDIV32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FIDIV DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FIDIV32_from_ptr ENDP

preserve_FIDIVR32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FIDIVR DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FIDIVR32_from_ptr ENDP

preserve_FLD_f32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FLD DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FLD_f32_from_ptr ENDP

preserve_FADD_f32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FADD DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FADD_f32_from_ptr ENDP

preserve_FMUL_f32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FMUL DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FMUL_f32_from_ptr ENDP

preserve_FDIV_f32_from_ptr PROC
    mov eax, [esp+4]
    pushad
    pushfd
    FDIV DWORD PTR [eax]
    popfd
    popad
    ret
preserve_FDIV_f32_from_ptr ENDP

END
