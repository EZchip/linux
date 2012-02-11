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
#define _KERNEL_PAGE_PERMS  (_PAGE_K_EXECUTE | _PAGE_K_WRITE |  _PAGE_K_READ)

#ifdef  CONFIG_ARC700_CACHE_PAGES
#define _PAGE_DEFAULT_CACHEABLE _PAGE_CACHEABLE
#else
#define _PAGE_DEFAULT_CACHEABLE (0)
#endif

/* ARC700 can do exclusive execute/write protection but most of the other
 * architectures implement it such that execute implies read permission
 * and write imples read permission. So to be compatible we do the same
 */
#define _PAGE_TABLE	(_PAGE_PRESENT \
			| _PAGE_ACCESSED | _PAGE_MODIFIED \
			| _KERNEL_PAGE_PERMS)
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_MODIFIED)
#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_DEFAULT_CACHEABLE \
			| _KERNEL_PAGE_PERMS)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE \
			| _PAGE_DEFAULT_CACHEABLE | _KERNEL_PAGE_PERMS)
#define PAGE_SHARED_EXECUTE	__pgprot(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE \
			        | _PAGE_EXECUTE | _PAGE_DEFAULT_CACHEABLE\
                                | _KERNEL_PAGE_PERMS)
#define	PAGE_COPY	__pgprot(_PAGE_PRESENT | _PAGE_READ \
			| _PAGE_DEFAULT_CACHEABLE | _KERNEL_PAGE_PERMS)
#define	PAGE_COPY_EXECUTE	__pgprot(_PAGE_PRESENT | _PAGE_READ \
				|_PAGE_EXECUTE | _PAGE_DEFAULT_CACHEABLE \
				| _KERNEL_PAGE_PERMS)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_READ \
			| _PAGE_DEFAULT_CACHEABLE | _KERNEL_PAGE_PERMS)
#define PAGE_READONLY_EXECUTE	__pgprot(_PAGE_PRESENT | _PAGE_READ \
				| _PAGE_EXECUTE | _PAGE_DEFAULT_CACHEABLE \
				| _KERNEL_PAGE_PERMS )
/* Kernel only page (vmalloc) : global page with all user mode perms
 * denied. kernel mode perms are set for all pages */

/* Disabling cacheable bit so vmalloc'ed pages will not be cached.
 * This is done to work around a problem in loading modules vmalloc area.
 * FIXME : Need to see how we can support caching of module pages by looking
 * at the module loading code. It does not seem to be flushing the Dcache */


#define _PAGE_KERNEL	(_PAGE_PRESENT | _PAGE_GLOBAL \
			| _PAGE_DEFAULT_CACHEABLE | _KERNEL_PAGE_PERMS)

#define _PAGE_KERNEL_NO_CACHE	(_PAGE_PRESENT | _PAGE_GLOBAL \
			|  _KERNEL_PAGE_PERMS)

#define PAGE_KERNEL	__pgprot(_PAGE_KERNEL)

#define _KERNPG_TABLE   (_PAGE_TABLE | _PAGE_KERNEL)
        /* xwr */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY_EXECUTE
#define __P101	PAGE_READONLY_EXECUTE
#define __P110	PAGE_COPY_EXECUTE
#define __P111	PAGE_COPY_EXECUTE

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY_EXECUTE
#define __S101	PAGE_READONLY_EXECUTE
#define __S110	PAGE_SHARED_EXECUTE
#define __S111	PAGE_SHARED_EXECUTE

#endif  /* _ASM_ARC_MMU_H */
