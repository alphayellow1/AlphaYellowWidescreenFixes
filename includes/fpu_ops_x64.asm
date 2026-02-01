; ============================================================
; fpu_ops_x64.asm
; Windows x64 MASM – x87 FPU helpers
; ml64.exe compatible
; ============================================================

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

.code

; ============================================================
; FILD loaders
; ============================================================

PUBLIC FILD16_from_ptr
FILD16_from_ptr PROC
    fild word ptr [rcx]
    ret
FILD16_from_ptr ENDP

PUBLIC FILD32_from_ptr
FILD32_from_ptr PROC
    fild dword ptr [rcx]
    ret
FILD32_from_ptr ENDP

PUBLIC FILD64_from_ptr
FILD64_from_ptr PROC
    fild qword ptr [rcx]
    ret
FILD64_from_ptr ENDP

; ============================================================
; FIADD
; ============================================================

PUBLIC FIADD16_from_ptr
FIADD16_from_ptr PROC
    fiadd word ptr [rcx]
    ret
FIADD16_from_ptr ENDP

PUBLIC FIADD32_from_ptr
FIADD32_from_ptr PROC
    fiadd dword ptr [rcx]
    ret
FIADD32_from_ptr ENDP

PUBLIC FIADD64_from_ptr
FIADD64_from_ptr PROC
    fild qword ptr [rcx]
    faddp st(1), st(0)
    ret
FIADD64_from_ptr ENDP

; ============================================================
; FISUB
; ============================================================

PUBLIC FISUB16_from_ptr
FISUB16_from_ptr PROC
    fisub word ptr [rcx]
    ret
FISUB16_from_ptr ENDP

PUBLIC FISUB32_from_ptr
FISUB32_from_ptr PROC
    fisub dword ptr [rcx]
    ret
FISUB32_from_ptr ENDP

PUBLIC FISUB64_from_ptr
FISUB64_from_ptr PROC
    fild qword ptr [rcx]
    fsubp st(1), st(0)
    ret
FISUB64_from_ptr ENDP

; ============================================================
; FISUBR
; ============================================================

PUBLIC FISUBR16_from_ptr
FISUBR16_from_ptr PROC
    fisubr word ptr [rcx]
    ret
FISUBR16_from_ptr ENDP

PUBLIC FISUBR32_from_ptr
FISUBR32_from_ptr PROC
    fisubr dword ptr [rcx]
    ret
FISUBR32_from_ptr ENDP

PUBLIC FISUBR64_from_ptr
FISUBR64_from_ptr PROC
    fild qword ptr [rcx]
    fsubrp st(1), st(0)
    ret
FISUBR64_from_ptr ENDP

; ============================================================
; FIMUL
; ============================================================

PUBLIC FIMUL16_from_ptr
FIMUL16_from_ptr PROC
    fimul word ptr [rcx]
    ret
FIMUL16_from_ptr ENDP

PUBLIC FIMUL32_from_ptr
FIMUL32_from_ptr PROC
    fimul dword ptr [rcx]
    ret
FIMUL32_from_ptr ENDP

PUBLIC FIMUL64_from_ptr
FIMUL64_from_ptr PROC
    fild qword ptr [rcx]
    fmulp st(1), st(0)
    ret
FIMUL64_from_ptr ENDP

; ============================================================
; FIDIV
; ============================================================

PUBLIC FIDIV16_from_ptr
FIDIV16_from_ptr PROC
    fidiv word ptr [rcx]
    ret
FIDIV16_from_ptr ENDP

PUBLIC FIDIV32_from_ptr
FIDIV32_from_ptr PROC
    fidiv dword ptr [rcx]
    ret
FIDIV32_from_ptr ENDP

PUBLIC FIDIV64_from_ptr
FIDIV64_from_ptr PROC
    fild qword ptr [rcx]
    fdivp st(1), st(0)
    ret
FIDIV64_from_ptr ENDP

; ============================================================
; FIDIVR
; ============================================================

PUBLIC FIDIVR16_from_ptr
FIDIVR16_from_ptr PROC
    fidivr word ptr [rcx]
    ret
FIDIVR16_from_ptr ENDP

PUBLIC FIDIVR32_from_ptr
FIDIVR32_from_ptr PROC
    fidivr dword ptr [rcx]
    ret
FIDIVR32_from_ptr ENDP

PUBLIC FIDIVR64_from_ptr
FIDIVR64_from_ptr PROC
    fild qword ptr [rcx]
    fdivrp st(1), st(0)
    ret
FIDIVR64_from_ptr ENDP

; ============================================================
; FICOMP
; ============================================================

PUBLIC FICOMP16_from_ptr
FICOMP16_from_ptr PROC
    ficomp word ptr [rcx]
    ret
FICOMP16_from_ptr ENDP

PUBLIC FICOMP32_from_ptr
FICOMP32_from_ptr PROC
    ficomp dword ptr [rcx]
    ret
FICOMP32_from_ptr ENDP

PUBLIC FICOMP64_from_ptr
FICOMP64_from_ptr PROC
    fild qword ptr [rcx]
    fxch st(1)
    fcomp st(1)
    fstp st(0)
    ret
FICOMP64_from_ptr ENDP

; ============================================================
; Trigonometry / misc
; ============================================================

PUBLIC FSIN_from_ptr
FSIN_from_ptr PROC
    fsin
    ret
FSIN_from_ptr ENDP

PUBLIC FCOS_from_ptr
FCOS_from_ptr PROC
    fcos
    ret
FCOS_from_ptr ENDP

PUBLIC FSINCOS_from_ptr
FSINCOS_from_ptr PROC
    fsincos
    ret
FSINCOS_from_ptr ENDP

PUBLIC FPTAN_from_ptr
FPTAN_from_ptr PROC
    fptan
    ret
FPTAN_from_ptr ENDP

PUBLIC FPATAN_from_ptr
FPATAN_from_ptr PROC
    fpatan
    ret
FPATAN_from_ptr ENDP

PUBLIC FPREM_from_ptr
FPREM_from_ptr PROC
    fprem
    ret
FPREM_from_ptr ENDP

PUBLIC FPREM1_from_ptr
FPREM1_from_ptr PROC
    fprem1
    ret
FPREM1_from_ptr ENDP

PUBLIC FYL2X_from_ptr
FYL2X_from_ptr PROC
    fyl2x
    ret
FYL2X_from_ptr ENDP

PUBLIC FYL2XP1_from_ptr
FYL2XP1_from_ptr PROC
    fyl2xp1
    ret
FYL2XP1_from_ptr ENDP

PUBLIC FSCALE_from_ptr
FSCALE_from_ptr PROC
    fscale
    ret
FSCALE_from_ptr ENDP

PUBLIC FSQRT_from_ptr
FSQRT_from_ptr PROC
    fsqrt
    ret
FSQRT_from_ptr ENDP

; ============================================================
; Float (32-bit)
; ============================================================

PUBLIC FLD_f32_from_ptr
FLD_f32_from_ptr PROC
    fld dword ptr [rcx]
    ret
FLD_f32_from_ptr ENDP

PUBLIC FADD_f32_from_ptr
FADD_f32_from_ptr PROC
    fadd dword ptr [rcx]
    ret
FADD_f32_from_ptr ENDP

PUBLIC FSUB_f32_from_ptr
FSUB_f32_from_ptr PROC
    fsub dword ptr [rcx]
    ret
FSUB_f32_from_ptr ENDP

PUBLIC FSUBR_f32_from_ptr
FSUBR_f32_from_ptr PROC
    fsubr dword ptr [rcx]
    ret
FSUBR_f32_from_ptr ENDP

PUBLIC FMUL_f32_from_ptr
FMUL_f32_from_ptr PROC
    fmul dword ptr [rcx]
    ret
FMUL_f32_from_ptr ENDP

PUBLIC FDIV_f32_from_ptr
FDIV_f32_from_ptr PROC
    fdiv dword ptr [rcx]
    ret
FDIV_f32_from_ptr ENDP

PUBLIC FDIVR_f32_from_ptr
FDIVR_f32_from_ptr PROC
    fdivr dword ptr [rcx]
    ret
FDIVR_f32_from_ptr ENDP

PUBLIC FCOMP_f32_from_ptr
FCOMP_f32_from_ptr PROC
    fcomp dword ptr [rcx]
    ret
FCOMP_f32_from_ptr ENDP

; ============================================================
; Double (64-bit)
; ============================================================

PUBLIC FLD_f64_from_ptr
FLD_f64_from_ptr PROC
    fld qword ptr [rcx]
    ret
FLD_f64_from_ptr ENDP

PUBLIC FADD_f64_from_ptr
FADD_f64_from_ptr PROC
    fadd qword ptr [rcx]
    ret
FADD_f64_from_ptr ENDP

PUBLIC FSUB_f64_from_ptr
FSUB_f64_from_ptr PROC
    fsub qword ptr [rcx]
    ret
FSUB_f64_from_ptr ENDP

PUBLIC FSUBR_f64_from_ptr
FSUBR_f64_from_ptr PROC
    fsubr qword ptr [rcx]
    ret
FSUBR_f64_from_ptr ENDP

PUBLIC FMUL_f64_from_ptr
FMUL_f64_from_ptr PROC
    fmul qword ptr [rcx]
    ret
FMUL_f64_from_ptr ENDP

PUBLIC FDIV_f64_from_ptr
FDIV_f64_from_ptr PROC
    fdiv qword ptr [rcx]
    ret
FDIV_f64_from_ptr ENDP

PUBLIC FDIVR_f64_from_ptr
FDIVR_f64_from_ptr PROC
    fdivr qword ptr [rcx]
    ret
FDIVR_f64_from_ptr ENDP

PUBLIC FCOMP_f64_from_ptr
FCOMP_f64_from_ptr PROC
    fcomp qword ptr [rcx]
    ret
FCOMP_f64_from_ptr ENDP

; ============================================================
; Preserve wrappers (x64 no-op equivalents)
; x87 does not clobber GPRs in x64
; ============================================================

PUBLIC preserve_FIADD32_from_ptr
preserve_FIADD32_from_ptr PROC
    fiadd dword ptr [rcx]
    ret
preserve_FIADD32_from_ptr ENDP

PUBLIC preserve_FISUB32_from_ptr
preserve_FISUB32_from_ptr PROC
    fisub dword ptr [rcx]
    ret
preserve_FISUB32_from_ptr ENDP

PUBLIC preserve_FIMUL32_from_ptr
preserve_FIMUL32_from_ptr PROC
    fimul dword ptr [rcx]
    ret
preserve_FIMUL32_from_ptr ENDP

PUBLIC preserve_FIDIV32_from_ptr
preserve_FIDIV32_from_ptr PROC
    fidiv dword ptr [rcx]
    ret
preserve_FIDIV32_from_ptr ENDP

PUBLIC preserve_FIDIVR32_from_ptr
preserve_FIDIVR32_from_ptr PROC
    fidivr dword ptr [rcx]
    ret
preserve_FIDIVR32_from_ptr ENDP

PUBLIC preserve_FLD_f32_from_ptr
preserve_FLD_f32_from_ptr PROC
    fld dword ptr [rcx]
    ret
preserve_FLD_f32_from_ptr ENDP

PUBLIC preserve_FADD_f32_from_ptr
preserve_FADD_f32_from_ptr PROC
    fadd dword ptr [rcx]
    ret
preserve_FADD_f32_from_ptr ENDP

PUBLIC preserve_FMUL_f32_from_ptr
preserve_FMUL_f32_from_ptr PROC
    fmul dword ptr [rcx]
    ret
preserve_FMUL_f32_from_ptr ENDP

PUBLIC preserve_FDIV_f32_from_ptr
preserve_FDIV_f32_from_ptr PROC
    fdiv dword ptr [rcx]
    ret
preserve_FDIV_f32_from_ptr ENDP

END