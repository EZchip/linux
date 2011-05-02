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

#ifdef __KERNEL__

/* Build option For chips with Metal Fix */
/* #define METAL_FIX  1 */
#define METAL_FIX  0

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

#if (CONFIG_ARC_MMU_VER >=2)

#define TLBWriteNI  0x5     // JH special -- write JTLB without inv uTLBs
#define TLBIVUTLB   0x6     //JH special -- explicitly inv uTLBs

#elif (METAL_FIX==1)   /* Metal Fix: Old MMU but a new Cmd */

#define TLBWriteNI  TLBWrite    // WriteNI doesn't exist on this H/w
#define TLBIVUTLB   0x5         // This is the only additional cmd

#else /* MMU V1 */

#undef TLBWriteNI       // These cmds don't exist on older MMU
#undef TLBIVUTLB

#endif

#define PTE_BITS_IN_PD0    (_PAGE_GLOBAL | _PAGE_VALID)
#define PTE_BITS_IN_PD1    (PAGE_MASK | \
                             _PAGE_CACHEABLE | \
                             _PAGE_EXECUTE | _PAGE_WRITE | _PAGE_READ | \
                             _PAGE_K_EXECUTE | _PAGE_K_WRITE | _PAGE_K_READ)

#ifndef __ASSEMBLY__

void arc_mmu_init(void);
void tlb_find_asid(unsigned int asid);
void __init read_decode_mmu_bcr(void);

#define tlb_flush(tlb) local_flush_tlb_mm((tlb)->mm)

#define tlb_start_vma(tlb, vma)                 \
    do {                            \
        if (!tlb->fullmm)               \
            flush_cache_range(vma, vma->vm_start, vma->vm_end); \
    }  while (0)

#define tlb_end_vma(tlb, vma) do { } while (0)

#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)

#include <linux/pagemap.h>
#include <asm-generic/tlb.h>

#else
#include <asm/tlb-mmu1.h>
#endif


#endif

#endif  /* _ASM_ARC_TLB_H */
