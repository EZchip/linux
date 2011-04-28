/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 *
 *
 *
 * Vineetg: Aug 13th 2008
 *  - Passing ECR (Exception Cause REG) to do_page_fault( ) for printing
 *    more information in case of a Fatality
 *
 * Vineetg: March 25th Bug #92690
 *  -Added Debug Code to check if sw-ASID == hw-ASID
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 *  arch/arc/mm/tlbex.S
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Rahul Trivedi, Amit Bhor
 *
 * TLB Exception Handling Code for A700
 */

    .cpu A7

#include <linux/linkage.h>
#include <asm/arcregs.h>
#include <asm/entry.h>
#include <asm/tlb.h>
#include <asm/traps.h>	/* DATA_TLB_STORE */
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/asm-offsets.h>
#include <asm/event-log-asm.h>

;--------------------------------------------------------------------------
; Temporary Variables used to Free UP a REG at the entry of Exception
; ARC 700 doesnt have any scratch REG which can be used to free up a REG
;   for low level stack switching and saving pre-exception REGS
; Thus need these globals.
; TODO: This wont work in SMP
;--------------------------------------------------------------------------
.section .bss

    .global ex_saved_reg1       ; Tmp holds r0 for duration of TLB Refill Code
    .align 4
    .type   ex_saved_reg1, @object
    .size   ex_saved_reg1, 4
ex_saved_reg1:
    .zero 4

    .global ex_saved_reg2       ; Tmp holds r1 for duration of TLB Refill Code
    .align 4
    .type   ex_saved_reg2, @object
    .size   ex_saved_reg2, 4
ex_saved_reg2:
    .zero 4

    .global mmu_write_cmd       ; TLBWrite or TLBWriteNI depending on MMU ver
    .align 4
    .type   mmu_write_cmd, @object
    .size   mmu_write_cmd, 4
mmu_write_cmd:
    .zero 4


.section .text, "ax",@progbits

;--------------------------------------------------------------------------
;  Troubleshooting Stuff
;--------------------------------------------------------------------------

; Linux keeps ASID (Address Space ID) in task->active_mm->context.asid
; When Creating TLB Entries, instead of doing 3 dependent loads from memory,
; we use the MMU PID Reg to get current ASID.
; In bizzare scenrios SW and HW ASID can get out-of-sync which is trouble.
; So we try to detect this in TLB Mis shandler


.macro DBG_ASID_MISMATCH

#ifdef CONFIG_ARC_TLB_PARANOIA

    ; make sure h/w ASID is same as s/w ASID

    GET_CURR_TASK_ON_CPU  r0
    ld r0, [r0, TASK_ACT_MM]
    ld r0, [r0, MM_CTXT+MM_CTXT_ASID]

    lr r1, [ARC_REG_PID]
    and r1, r1, ASID_EXTRACT_MASK
    cmp r1, r0
    beq 5f

    ; H/w and S/w ASId don't match, but we maybe in kernel
    lr  r0, [erstatus]
    and.f   0, r0, STATUS_U_MASK
    bz  5f  ;In kernel so not error

    ; We sure are in troubled waters, Flag the error, but to do so
    ; need to switch to kernel mode stack to call error routine
    GET_CURR_TASK_ON_CPU  sp
    ld  sp, [sp, TASK_THREAD_INFO]
    add sp, sp, ( 0x2000 - 4 )

    ; Call printk to shoutout aloud
    mov r0, 1
    j print_asid_mismatch

5:   ; ASIDs match so proceed normally
    nop

#endif

.endm

;-----------------------------------------------------------------
; Linux keeps Page Directory Pointer in task->active_mm->pdg
; To save the 3 dependent loads, we cache it in MMU_SCRATCH Reg
;
; This code verifies the SW and HW PGD are same for current task.

.macro DBG_PDG_MISMATCH

#ifdef CONFIG_ARC_TLB_PARANOIA

    lr r0, [ARC_REG_SCRATCH_DATA0]
    GET_CURR_TASK_ON_CPU  r1
    ld r1, [r1, TASK_ACT_MM]
    ld r1, [r1, MM_PGD]
    cmp  r1, r0
    beq  59f

    ; We sure are in troubled waters, Flag the error, but to do so
    ; need to switch to kernel mode stack to call error routine
    GET_CURR_TASK_ON_CPU  sp
    ld  sp, [sp, TASK_THREAD_INFO]
    add sp, sp, ( 0x2000 - 4 )

    j print_pgd_mismatch

59:

#endif

.endm


;-----------------------------------------------------------------------------
;TLB Miss handling Code
;-----------------------------------------------------------------------------

/* This macro does the page-table lookup for the faulting address.
 * At the end of the macro pte faulted on will be in register pte
 * and the table in which pte belongs will be in ptr.
 */
.macro LOAD_PTE pte, ptr, label

    lr  \pte, [efa]          /* faulting address */

#ifndef CONFIG_SMP
    lr  \ptr, [ARC_REG_SCRATCH_DATA0] /* current pgd */
#else
    GET_CURR_TASK_ON_CPU  \ptr
    ld  \ptr, [\ptr, TASK_ACT_MM]
    ld  \ptr, [\ptr, MM_PGD]
#endif

    lsr \pte, \pte, PGDIR_SHIFT         /* get pgd offset */
    asl \pte, \pte, 2     /* multiply by 4 as each entry is 4 bytes */
    ld  \ptr, [\ptr, \pte]     /* get ptebase */
    and.f   \ptr, \ptr, PAGE_MASK /* Ignoring protection and other flags */
    bz  \label      /* if the pgd entry is 0 do page fault */
    lr  \pte, [efa]          /* load faulting address once again */
    lsr \pte, \pte, PAGE_SHIFT        /* get pte offset */
    and \pte, \pte, 0x7FF      /* page table - 11 bits */
    asl \pte, \pte, 2        /* multiply by 4 */
    ld.a    \pte, [\ptr, \pte]    /* get the pte entry faulted on and
                    the table on which it faulted in ptr */
.endm

.macro PTE_PRESENT pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_READ)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_READ)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_K_PRESENT pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_K_READ)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_K_READ)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_PRESENTEXEC pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_READ | _PAGE_EXECUTE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_READ | _PAGE_EXECUTE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_K_PRESENTEXEC pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_K_READ | _PAGE_K_EXECUTE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_K_READ | _PAGE_K_EXECUTE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_READWRITABLE pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_K_READWRITABLE pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_K_READ | _PAGE_K_WRITE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_K_READ | _PAGE_K_WRITE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_WRITABLE pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_WRITE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_WRITE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_K_WRITABLE pte, ptr, label
    and \pte, \pte,(_PAGE_PRESENT | _PAGE_K_WRITE)
    xor.f   \pte, \pte, (_PAGE_PRESENT | _PAGE_K_WRITE)
    bnz \label
    ld  \pte, [\ptr, 0]
.endm

.macro PTE_MAKEVALID pte, ptr
    or  \pte, \pte, (_PAGE_VALID | _PAGE_ACCESSED)
    st  \pte, [\ptr, 0]
.endm

.macro PTE_MAKEWRITE pte, ptr
    or  \pte, \pte, (_PAGE_VALID | _PAGE_ACCESSED | _PAGE_MODIFIED)
    st  \pte, [\ptr, 0]
.endm

.macro DO_FAULT write
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    ; take a snapshot of upon entering SLOW Path TLB Hdlr
    TAKE_SNAP_ASM r8, r9, SNAP_DO_PF_ENTER

    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    mov r1, \write
    lr  r2, [efa]
    lr  r3, [ecr]
    mov r0, sp

    ; We dont want exceptions to be disabled while the fault is handled.
    ; Now that we have saved the context we return from exception hence
    ; exceptions get re-enable

    FAKE_RET_FROM_EXCPN  r9

    jl  do_page_fault
    j   ret_from_user_faults
.endm

.macro TLB_RELOAD pte, ptr
    ld  \pte, [\ptr, 0]
    and \ptr, \pte, 0xe00    /* get G,L,V bits from PTE */
    lsr \ptr, \ptr           /* right shift by 1 */
    and \pte, \pte, TLBPD1_MASK  /* mask out unused bits */
    sr  \pte, [ARC_REG_TLBPD1]
    lr  \pte,[ARC_REG_TLBPD0]
    or  \ptr,\ptr,\pte
    sr  \ptr,[ARC_REG_TLBPD0]
.endm

; This macro essentially
.macro TLB_WRITE_DATA

#define JH_HACK1
//#define JH_HACK2
//#define JH_HACK3
#ifdef JH_HACK3
    ; Calculate set index for 2-way MMU
    ; - avoiding use of GetIndex from MMU
    ;   and its unpleasant LFSR pseudo-random sequence
    ;
    ; r1 = TLBPD0 from TLB_RELOAD above
    ;
    ; -- jh_ex_way_set not cleared on startup
    ;    didn't want to change setup.c
    ;    hence extra instruction to clean
    ;
    ; -- should be in cache since in same line
    ;    as r0/r1 saves above
    ;
    ld  r0,[jh_ex_way_sel]  ; victim pointer
    and r0,r0,1         ; clean
    xor.f   r0,r0,1         ; flip
    st  r0,[jh_ex_way_sel]  ; store back
    asr r0,r1,12        ; get set # <<1, note bit 12=R=0
    or.nz   r0,r0,1         ; set way bit
    and r0,r0,0xff      ; clean
    sr  r0,[ARC_REG_TLBINDEX]
#endif
#ifdef JH_HACK2
; JH hack #2
;  Faster than hack #1 in non-thrash case, but hard-coded for 2-way MMU
;  Slower in thrash case (where it matters) because more code is executed
;  Inefficient due to two-register paradigm of this miss handler
;
        /* r1 = data TLBPD0 at this point */
        lr      r0,[eret]               /* instruction address */
        xor     r0,r0,r1                /* compare set #       */
        and.f   r0,r0,0x000fe000        /* 2-way MMU mask      */
        bne     88f                     /* not in same set - no need to probe */

        lr      r0,[eret]               /* instruction address */
        and     r0,r0,PAGE_MASK         /* VPN of instruction address */
;       lr      r1,[ARC_REG_TLBPD0]     /* Data VPN+ASID - already in r1 from TLB_RELOAD*/
        and     r1,r1,0xff              /* Data ASID */
        or      r0,r0,r1                /* Instruction address + Data ASID */

        lr      r1,[ARC_REG_TLBPD0]     /* save TLBPD0 containing data TLB*/
        sr      r0,[ARC_REG_TLBPD0]     /* write instruction address to TLBPD0 */
        sr  TLBProbe, [ARC_REG_TLBCOMMAND] /* Look for instruction */
        lr      r0,[ARC_REG_TLBINDEX]   /* r0 = index where instruction is, if at all */
        sr      r1,[ARC_REG_TLBPD0]     /* restore TLBPD0 */

        xor     r0,r0,1                 /* flip bottom bit of data index */
        b.d     89f
        sr      r0,[ARC_REG_TLBINDEX]   /* and put it back */
88:
    sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]
89:
#endif
#ifdef JH_HACK1
    //
    // Always checks whether instruction will be kicked out by dtlb miss
    //
        lr      r0,[eret]               /* instruction address */
        and     r0,r0,PAGE_MASK         /* VPN of instruction address */
;       lr      r1,[ARC_REG_TLBPD0]     /* Data VPN+ASID - already in r1 from TLB_RELOAD*/
        and     r1,r1,0xff              /* Data ASID */
        or      r0,r0,r1                /* Instruction address + Data ASID */

        lr      r1,[ARC_REG_TLBPD0]     /* save TLBPD0 containing data TLB*/
        sr      r0,[ARC_REG_TLBPD0]     /* write instruction address to TLBPD0 */
        sr  TLBProbe, [ARC_REG_TLBCOMMAND] /* Look for instruction */
        lr      r0,[ARC_REG_TLBINDEX]   /* r0 = index where instruction is, if at all */
        sr      r1,[ARC_REG_TLBPD0]     /* restore TLBPD0 */

        sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]
        lr      r1,[ARC_REG_TLBINDEX]   /* r1 = index where MMU wants to put data */
        brne    r0,r1,88f               /* if no match on indices, go around */
        xor     r1,r1,1                 /* flip bottom bit of data index */
        sr      r1,[ARC_REG_TLBINDEX]   /* and put it back */
88:
#endif
#if !(defined(JH_HACK1)|defined(JH_HACK2)|defined(JH_HACK3))
    /* Here i am not checking if the index returned is error */
    /* I assume that i will always get an index to the store the TLB entry */
    /* i.e. there will be atleast one way that is unlocked */
    sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]
#endif

    COMMIT_WRITE_CMD
.endm

.macro TLB_WRITE_INSTR

    /* Ask MMU to get us a free Index */
    sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]

    /* Commit the Write */
    COMMIT_WRITE_CMD
.endm

.macro COMMIT_WRITE_CMD
    ld  r0, [SYMBOL_NAME(mmu_write_cmd)]
    sr  r0, [ARC_REG_TLBCOMMAND]
.endm

.macro SAVE_TEMP
    st  r0, [ex_saved_reg1]
    st  r1, [ex_saved_reg2]

    ; take a snapshot of upon entering FAST Path TLB Hdlr
    TAKE_SNAP_ASM r0, r1, SNAP_EXCP_IN

    ; VERIFY if the ASID in MMU-PID Reg is same as
    ; one in Linux data structures

    DBG_ASID_MISMATCH

    ; VERIFY if the cached PGD ptr in MMU-SCRATCH Reg is same as
    ; one in Linux data structures

	DBG_PDG_MISMATCH
.endm

.macro RESTORE_TEMP
    ld  r0, [ex_saved_reg1]
    ld  r1, [ex_saved_reg2]
.endm

; branch range is -255 to 256 hence won't work with inline debug code
; Need a jmp costing an extra instruction in Fast Path

.macro BR_OR_JMP_IF_HS  reg,  val, addr

#ifdef CONFIG_ARC_TLB_PARANOIA
    cmp     \reg, \val
    jhs    	\addr
#else
    brhs    \reg, \val, \addr
#endif
.endm

ARC_ENTRY EV_TLBMissI

    SAVE_TEMP

    lr  r0, [efa]
    BR_OR_JMP_IF_HS	r0, VMALLOC_START, kernel_inst_miss

    LOAD_PTE r0, r1, no_page_itlb
    PTE_PRESENTEXEC r0, r1, no_page_itlb
reload_tlb_inst_miss:
    PTE_MAKEVALID r0, r1
    TLB_RELOAD r0, r1
    TLB_WRITE_INSTR
    RESTORE_TEMP
    rtie
kernel_inst_miss:
    LOAD_PTE r0, r1, no_page_itlb
    PTE_K_PRESENTEXEC r0, r1, no_page_itlb
    b   reload_tlb_inst_miss
no_page_itlb:
    RESTORE_TEMP
    DO_FAULT 0

SYMBOL_NAME_LABEL(data_tlb_miss_load)

    lr  r0, [efa]
    BR_OR_JMP_IF_HS	r0, VMALLOC_START, kernel_data_miss_load

    LOAD_PTE r0, r1, no_page_dtlb_load
    PTE_PRESENT r0, r1, no_page_dtlb_load
reload_tlb_data_miss_load:
    PTE_MAKEVALID r0, r1
    TLB_RELOAD r0, r1
    TLB_WRITE_DATA
    RESTORE_TEMP
    rtie
kernel_data_miss_load:
    LOAD_PTE r0, r1, no_page_dtlb_load
    PTE_K_PRESENT r0, r1, no_page_dtlb_load
    b   reload_tlb_data_miss_load
no_page_dtlb_load:
    RESTORE_TEMP
    DO_FAULT 0

SYMBOL_NAME_LABEL(data_tlb_miss_store)

    lr  r0, [efa]
    BR_OR_JMP_IF_HS	r0, VMALLOC_START, kernel_data_miss_store

    LOAD_PTE r0, r1, no_page_dtlb_store
    PTE_WRITABLE r0, r1, no_page_dtlb_store
reload_tlb_data_miss_store:
    PTE_MAKEWRITE r0, r1
    TLB_RELOAD r0, r1
    TLB_WRITE_DATA
    RESTORE_TEMP
    rtie
kernel_data_miss_store:
    LOAD_PTE r0, r1, no_page_dtlb_store
    PTE_K_WRITABLE r0, r1, no_page_dtlb_store
    b   reload_tlb_data_miss_store
no_page_dtlb_store:
    RESTORE_TEMP
    DO_FAULT 1

SYMBOL_NAME_LABEL(data_tlb_miss_xchg)

    lr  r0, [efa]
    BR_OR_JMP_IF_HS	r0, VMALLOC_START, kernel_data_miss_xchg

    LOAD_PTE r0, r1, no_page_dtlb_xchg
    PTE_READWRITABLE r0, r1, no_page_dtlb_xchg
reload_tlb_data_miss_xchg:
    PTE_MAKEWRITE r0, r1
    TLB_RELOAD r0, r1
    TLB_WRITE_DATA
    RESTORE_TEMP
    rtie
kernel_data_miss_xchg:
    LOAD_PTE r0, r1, no_page_dtlb_xchg
    PTE_K_READWRITABLE r0, r1, no_page_dtlb_xchg
    b   reload_tlb_data_miss_xchg
no_page_dtlb_xchg:
    RESTORE_TEMP
    DO_FAULT 1

ARC_ENTRY EV_TLBMissD

    SAVE_TEMP

    lr  r0, [ecr]
    and r0, r0, ECAUSE_CODE_MASK
    brne    r0, DATA_TLB_LOAD, not_data_load
    j   data_tlb_miss_load
not_data_load:
    brne    r0, DATA_TLB_STORE, not_data_store
    j   data_tlb_miss_store
not_data_store:
    j   data_tlb_miss_xchg
