;------------------------------------------------------------------------------
;
; Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   VmgExitSvsm.Asm
;
; Abstract:
;
;   AsmVmgExitSvsm function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; AsmVmgExitSvsm (
;   UINT64  Rcx,
;   UINT64  Rdx,
;   UINT64  R8,
;   UINT64  R9,
;   UINT64  Rax
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmVmgExitSvsm)
ASM_PFX(AsmVmgExitSvsm):
;
; NASM doesn't support the vmmcall instruction in 32-bit mode and NASM versions
; before 2.12 cannot translate the 64-bit "rep vmmcall" instruction into elf32
; format. Given that VMGEXIT does not make sense on IA32, provide a stub
; implementation that is identical to CpuBreakpoint(). In practice,
; AsmVmgExitSvsm() should never be called on IA32.
;
    int  3
    ret

