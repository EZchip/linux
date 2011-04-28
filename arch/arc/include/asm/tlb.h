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
 *  linux/include/asm-arc/tlb.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Autors : Amit Bhor, Rahul Trivedi
 */

#ifndef _ASM_ARC_TLB_H
#define _ASM_ARC_TLB_H

/* Build option For chips with Metal Fix */
/* #define METAL_FIX  1 */
#define METAL_FIX  0


/*
 * .. because we flush the whole mm when it fills up.
 */
#define tlb_flush(tlb) local_flush_tlb_mm((tlb)->mm)



#define TLB_SIZE 256                    // Num of TLB Entries in ARC700 MMU
#define ENTIRE_TLB_MAP  (TLB_SIZE * PAGE_SIZE)  // addr space mapped by TLB

#define TLB_LKUP_ERR    0x80000000      // Error code if Probe Fails
#define MMU_VER_2       2


/*************************************
 * Basic TLB Commands
 ************************************/
#define TLBWrite    0x1
#define TLBRead     0x2
#define TLBGetIndex 0x3
#define TLBProbe    0x4

/*************************************
 * New Cmds because of MMU Changes
 *************************************/

#if (METAL_FIX==0)      /* MMU v2 or V1 */

#define TLBWriteNI  0x5     // JH special -- write JTLB without inv uTLBs
#define TLBIVUTLB   0x6     //JH special -- explicitly inv uTLBs

#else   /* Metal Fix: Old MMU but a new Cmd */

#define TLBWriteNI  TLBWrite    // WriteNI doesn't exist on this H/w
#define TLBIVUTLB   0x5         // This is the only additional cmd

#endif


#ifndef __ASSEMBLY__
void tlb_init(void);
void tlb_find_asid(unsigned int asid);
#endif

/* Sameer: Do nothing as in MIPS */
#define tlb_start_vma(tlb, vma)                 \
    do {                            \
        if (!tlb->fullmm)               \
            flush_cache_range(vma, vma->vm_start, vma->vm_end); \
    }  while (0)

#define tlb_end_vma(tlb, vma) do { } while (0)

#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)

#ifndef __ASSEMBLY__
#include <linux/pagemap.h>
#include <asm-generic/tlb.h>
#endif

/* MMU PID Register contains 1 bit for MMU Enable and 8 bits of ASID */
#define MMU_ENABLE          0x80000000
#define ASID_EXTRACT_MASK   (~MMU_ENABLE_BIT)

#endif  /* _ASM_ARC_TLB_H */
