/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * vineetg: April 2011 :
 *  -MMU v3: PD bits slightly different
 *
 * Vineetg: July 2009
 *  - For MMU V2, we need not do heuristics at the time of commiting a D-TLB
 *      entry, so that it doesn't knock out it's I-TLB entry
 *  - Some more fine tuning:
 *      bmsk instead of add, asl.cc instead of branch, delay slot utilise etc
 *
 * Vineetg: July 2009
 *  - Practically rewrote the I/D TLB Miss handlers
 *    Now 40 and 135 instructions a peice as compared to 131 and 449 resp.
 *    Hence Leaner by 1.5 K
 *    Used Conditional arithmetic to replace excessive branching
 *    Also used short instructions wherever possible
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
#include <asm/entry.h>
#include <asm/tlb.h>
#include <asm/traps.h>
#include <asm/pgtable.h>
#include <asm/event-log-asm.h>

;--------------------------------------------------------------------------
; Temporary Variables used to Free UP a REG at the entry of Exception
; ARC 700 doesnt have any scratch REG which can be used to free up a REG
;   for low level stack switching and saving pre-exception REGS
; Thus need these globals.
; TODO: This wont work in SMP
;--------------------------------------------------------------------------
.section  .data.arcfp
    .global ex_saved_reg1       ; Holds r0-r3 for duration of TLB Refill Code
    .align 32                   ; IMP: Must be Cache Line aligned
    .type   ex_saved_reg1, @object
    .size   ex_saved_reg1, 16
ex_saved_reg1:
    .zero 4

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
    add sp, sp, ( THREAD_SIZE - 4 )

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
    add sp, sp, ( THREAD_SIZE - 4 )

    j print_pgd_mismatch

59:

#endif

.endm


;-----------------------------------------------------------------------------
;TLB Miss handling Code
;-----------------------------------------------------------------------------

; This macro does the page-table lookup for the faulting address.
; OUT: r0 = PTE faulted on, r1 = ptr to PTE, r2 = Faulting V-address
.macro LOAD_PTE

    lr  r2, [efa]

#ifndef CONFIG_SMP
    lr  r1, [ARC_REG_SCRATCH_DATA0] ; current pgd
#else
    GET_CURR_TASK_ON_CPU  r1
    ld  r1, [r1, TASK_ACT_MM]
    ld  r1, [r1, MM_PGD]
#endif

    lsr     r0, r2, PGDIR_SHIFT     ; Bits for indexing into PGD
    ld.as   r1, [r1, r0]            ; PGD entry corresp to faulting addr
    and.f   r1, r1, PAGE_MASK       ; Ignoring protection and other flags
                                    ;   contains Ptr to Page Table
    bz.d    do_slow_path_pf         ; if no Page Table, do page fault

    ; Get the PTE entry: The idea is
    ; (1) x = addr >> PAGE_SHIFT  => to mask out page offset bits from @fault-addr
    ; (2) y = x & (PTRS_IN_PGD - 1) = > to get index
    ; (3) z = pgtbl[y]
    ; To avoid the multiply by in end, we do the -2, <<2 below

    lsr     r0, r2, (PAGE_SHIFT - 2)
    and     r0, r0, ( (PTRS_PER_PTE-1) << 2)
    ld.aw   r0, [r1, r0]            ; get the pte entry and pte ptr corresp to
                                    ;    faulted addr
#ifdef CONFIG_ARC_TLB_PROFILE
    bbit1 r0, 0, 1f
    ld   r2, [num_pte_not_present]
    add r2, r2, 1
    st   r2, [num_pte_not_present]
1:
#endif

.endm


; Based on access type, check if PTE is valid
; IN: r0 = PTE
.macro VERIFY_PTE

    ; if PTE not present, bail out early
    bbit0 r0, 0, do_slow_path_pf

    mov_s   r2, 0
    lr      r3, [ecr]
    btst_s  r3,  DTLB_LD_MISS_BIT
    or.nz   r2, r2, _PAGE_READ      ; If Read access, check for Read flag in PTE
    btst_s  r3,  DTLB_ST_MISS_BIT
    or.nz   r2, r2, _PAGE_WRITE     ; If Write access, chk if Write flag in PTE
                                    ; Above laddering takes care of XCHG access
                                    ;   which is both Read and Write

    ; If kernel mode access, ; make _PAGE_xx flags as _PAGE_K_xx
    ; This code doesn't hit, when exception happens in kernel mode, when
    ; doing copy_to_user( ), as EFA would still have the user mode address
    ; ( < 0x7000_0000) where kernel faulted.
    ; This is correct, since there is nothing in user mode which kernel can't access.
    ; What this checks for is faults in vmalloc region (0x7000_0000 to 0x7FFF_FFFF)

    lr      r3, [efa]
    cmp     r3, VMALLOC_START - 1   ; If kernel mode access
    asl.hi  r2, r2, 3               ; make _PAGE_xx flags as _PAGE_K_xx
    or      r2, r2, _PAGE_PRESENT   ; Common flag for K/U mode

    ; By now, r2 setup with all the Flags we need to check in PTE
    and     r3, r0, r2              ; Mask out NON Flag bits from PTE
    brne.d  r3, r2, do_slow_path_pf ; check ( ( pte & flags_test ) == flags_test )
.endm

; Let Linux VM know that page was accessed/dirty
; IN: r0 = PTE, r1 = ptr to PTE
.macro UPDATE_PTE
    lr      r3, [ecr]
    or      r0, r0, (_PAGE_VALID | _PAGE_ACCESSED)  ; Set Accessed bit for all
    btst_s  r3,  DTLB_ST_MISS_BIT         ; See if it was a Write Access ?
    or.nz   r0, r0, _PAGE_MODIFIED        ; if Write, set Dirty bit as well
    st_s    r0, [r1]                      ; Write back PTE
.endm

; Setup PTE bits in TLB entry
; IN: r0 = PTE
.macro TLB_RELOAD
    and r1, r0, PTE_BITS_IN_PD0 ; Valid, Global
#if (CONFIG_ARC_MMU_VER <= 2)   // Neednot be done with v3 onwards
    lsr r1, r1                  ; shift PTE flags to match layout in PD0
#endif
    and r0, r0, PTE_BITS_IN_PD1 ; from PTE, mask out above flags
    sr  r0, [ARC_REG_TLBPD1]    ; Phy addr and access permission flags in PD1

    lr  r0,[ARC_REG_TLBPD0]     ; MMU prepares PD0 with vaddr and ASID
    or  r1, r1, r0              ; Stuff the above flags there
    sr  r1,[ARC_REG_TLBPD0]     ; rewrite PD0
.endm

.macro TLB_WRITE_HEURISTICS

#if (METAL_FIX || (CONFIG_ARC_MMU_VER == 1))

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
        mov_s   r3, r1                  ; save PD0 prepared by TLB_RELOAD in r3
        lr      r0,[eret]               /* instruction address */
        and     r0,r0,PAGE_MASK         /* VPN of instruction address */
        bmsk    r1,r3,7                 /* Data ASID, bits 7-0 */
        or_s    r0,r0,r1                /* Instruction address + Data ASID */

        sr      r0,[ARC_REG_TLBPD0]     /* write instruction address to TLBPD0 */
        sr      TLBProbe, [ARC_REG_TLBCOMMAND] /* Look for instruction */
        lr      r0,[ARC_REG_TLBINDEX]   /* r0 = index where instruction is, if at all */
        sr      r3,[ARC_REG_TLBPD0]     /* restore TLBPD0 */

        sr      TLBGetIndex, [ARC_REG_TLBCOMMAND]
        lr      r1,[ARC_REG_TLBINDEX]   /* r1 = index where MMU wants to put data */
        cmp     r0,r1                   /* if no match on indices, go around */
        xor.eq  r1,r1,1                 /* flip bottom bit of data index */
        sr      r1,[ARC_REG_TLBINDEX]   /* and put it back */
#endif
#else
    ; No need for heuristics in way detemination. We have newer MMU or fixed MMU

    sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]

#endif
.endm

.macro TLB_WRITE_DATA

    ; MMU with 2 way set assoc J-TLB, needs some help in pathetic case of
    ; memcpy where 3 parties contend for 2 ways, ensuing a livelock.
    ; But only for old MMU or one without Metal Fix
    TLB_WRITE_HEURISTICS

    COMMIT_WRITE_CMD
.endm

.macro TLB_WRITE_INSTR

    /* Ask MMU to get us a free Index */
    sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]

    /* Commit the Write */
    COMMIT_WRITE_CMD
.endm

.macro COMMIT_WRITE_CMD
#if (CONFIG_ARC_MMU_VER >= 2)   // introduced in v2
    sr TLBWriteNI, [ARC_REG_TLBCOMMAND]
#else
    sr TLBWrite, [ARC_REG_TLBCOMMAND]
#endif
.endm

.macro SAVE_TEMP
    st    r0, [ex_saved_reg1]
    mov_s r0, ex_saved_reg1
    st_s  r1, [r0, 4]
    st_s  r2, [r0, 8]
    st_s  r3, [r0, 12]

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
    mov_s r0, ex_saved_reg1
    ld_s  r3, [r0,12]
    ld_s  r2, [r0, 8]
    ld_s  r1, [r0, 4]
    ld_s  r0, [r0]
.endm

;-----------------------------------------------------------------------------
;I-TLB Miss handling Code
;-----------------------------------------------------------------------------

.section .text.arcfp, "ax",@progbits    ;Fast Path Code, candidate for ICCM

;-----------------------------------------------------------------------------
;I-TLB Miss handling Code
;-----------------------------------------------------------------------------

ARC_ENTRY EV_TLBMissI

    SAVE_TEMP

#ifdef CONFIG_ARC_TLB_PROFILE
    ld  r0, [numitlb]
    add r0, r0, 1
    st  r0, [numitlb]
#endif

    LOAD_PTE

    ; Check if PTE flags approp for executing code
    cmp_s   r2, VMALLOC_START
    mov.lo  r2, (_PAGE_PRESENT | _PAGE_READ | _PAGE_EXECUTE)
    mov.hs  r2, (_PAGE_PRESENT | _PAGE_K_READ | _PAGE_K_EXECUTE)

    and     r3, r0, r2  ; Mask out NON Flag bits from PTE
    xor.f   r3, r3, r2  ; check ( ( pte & flags_test ) == flags_test )
    bnz     do_slow_path_pf

    ; Let Linux VM know that the page was accessed
    or      r0, r0, (_PAGE_VALID | _PAGE_ACCESSED)  ; set Accessed Bit
    st_s    r0, [r1]                                ; Write back PTE

    TLB_RELOAD
    TLB_WRITE_INSTR
    RESTORE_TEMP
    rtie

ARC_EXIT EV_TLBMissI

;-----------------------------------------------------------------------------
;D-TLB Miss handling Code
;-----------------------------------------------------------------------------

ARC_ENTRY EV_TLBMissD

    SAVE_TEMP

#ifdef CONFIG_ARC_TLB_PROFILE
    ld  r0, [numdtlb]
    add r0, r0, 1
    st  r0, [numdtlb]
#endif

    ; Get the PTE corresponding to V-addr accessed
    ; If PTE exists, it will setup, r0 = PTE, r1 = Ptr to PTE
    LOAD_PTE

    VERIFY_PTE

    UPDATE_PTE

dtlb_commit_stage:
    TLB_RELOAD
    TLB_WRITE_DATA
    RESTORE_TEMP
    rtie

;-------- Common routine to call Linux Page Fault Handler -----------
do_slow_path_pf:
    RESTORE_TEMP
    st  r9, [SYMBOL_NAME(ex_saved_reg1)]

    ; take a snapshot of upon entering SLOW Path TLB Hdlr
    TAKE_SNAP_ASM r8, r9, SNAP_DO_PF_ENTER

    lr  r9, [erstatus]

    SWITCH_TO_KERNEL_STK
    SAVE_ALL_SYS

    ; ------- setup args for Linux Page fault Hanlder ---------
    mov_s r0, sp
    lr  r2, [efa]
    lr  r3, [ecr]

    ; Both st and ex imply WRITE access of some sort, hence do_page_fault( )
    ; invoked with write=1 for DTLB-st/ex Miss and write=0 for ITLB miss or
    ; DTLB-ld Miss
    ; DTLB Miss Cause code is ld = 0x01 , st = 0x02, ex = 0x03
    ; Following code cunningly uses that fact that st/ex have one bit in common

    btst_s r3, DTLB_ST_MISS_BIT
    mov.z  r1, 0
    mov.nz r1, 1

    ; We dont want exceptions to be disabled while the fault is handled.
    ; Now that we have saved the context we return from exception hence
    ; exceptions get re-enable

    FAKE_RET_FROM_EXCPN  r9

    bl  do_page_fault
    b   ret_from_exception


ARC_EXIT EV_TLBMissD

ARC_ENTRY EV_TLBMissB   ; Bogus entry to measure sz of DTLBMiss hdlr
