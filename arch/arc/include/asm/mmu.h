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
 *  linux/include/asm-arc/mmu.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_MMU_H
#define _ASM_ARC_MMU_H

/* Arch specific mmu context. We store the allocated ASID in here.
 */
#ifndef __ASSEMBLY__
#include <asm/unaligned.h>
typedef struct {
    unsigned long asid;
#ifdef CONFIG_ARC_TLB_DBG
    struct task_struct *tsk;
#endif
} mm_context_t;

#endif
/*  In memory ARC700 linux PTE's are 4 bytes wide as follows :
 *
 *  31            13|12      8        5      3      0
 *   -------------------------------------------------
 *  |       PFN     |M|V|L|G|Rk|Wk|Ek|Ru|Wu|Eu|Fc|A|P |
 *   -------------------------------------------------
 *
 *  Rk, Ek and Wk are not used by linux. The kernel has all permissions
 *  hence they are each set to 1.
 *  M -> Modified(dirty) , A -> Accessed , P -> Present are implemented
 *  in software.
 *  V->Valid, L->Locked, G->Global are kept here and copied into TLBPD0
 *  when updating the TLB.
 */

/* In memory PTE bits (shown above).
 * (S) -> Bits implemented in software
 * (H) -> Bits provided by hardware
 * _PAGE_EXECUTE not used currently
 */
#define _PAGE_PRESENT       (1<<0)  /* Page present in memory (S)*/
#define _PAGE_ACCESSED      (1<<1)  /* Page is accesses (S) */
#define _PAGE_CACHEABLE     (1<<2)  /* Page is cached (H) */
#define _PAGE_EXECUTE       (1<<3)  /* Page has user execute perm (H) */
#define _PAGE_WRITE         (1<<4)  /* Page has user write perm (H) */
#define _PAGE_SILENT_WRITE  (1<<4)  /* synonym of above */
#define _PAGE_READ          (1<<5)  /* Page has user read perm (H) */
#define _PAGE_SILENT_READ   (1<<5)  /* synonym of above */
#define _PAGE_K_EXECUTE     (1<<6)  /* Page has kernel execute perm (H) */
#define _PAGE_K_WRITE       (1<<7)  /* Page has kernel write perm (H) */
#define _PAGE_K_READ        (1<<8)  /* Page has kernel perm (H) */
#define _PAGE_GLOBAL        (1<<9)  /* Page is global (H) */
#define _PAGE_LOCKED        (1<<10) /* Page is locked in TLB (H) */
#define _PAGE_VALID         (1<<11) /* Page is valid (H) */
#define _PAGE_MODIFIED      (1<<12) /* Page modified (dirty) (S) */
/* Sameer: Temporary definition */
#define _PAGE_FILE          (1<<12) /* page cache/ swap (S) */

/* Kernel allowed all permissions for all pages */
#define _KERNEL_PAGE_PERMS  (_PAGE_K_EXECUTE | _PAGE_K_WRITE | \
                    _PAGE_K_READ)

#ifdef  CONFIG_ARC700_CACHE_PAGES
#define _PAGE_DEFAULT_CACHEABLE _PAGE_CACHEABLE
#else
#define _PAGE_DEFAULT_CACHEABLE (0)
#endif

/* ARC700 specifies that addr above 0x8000_0000 are not
 * translated by the MMU (reserved for kernel)
 * Note that even if kernel is reloacted to a higer addr
 * this number MUST NOT be changed
 */
#define ARC_UNTRANS_ADDR_START  0x80000000

#endif  /* _ASM_ARC_MMU_H */
