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
 *  include/asm-arc/page.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */
#ifndef __ASM_ARC_PAGE_H
#define __ASM_ARC_PAGE_H

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT  13

#ifdef __ASSEMBLY__
#define PAGE_SIZE       (1 << PAGE_SHIFT)
#define PAGE_OFFSET  	(0x80000000)
#else
#define PAGE_SIZE       (1UL << PAGE_SHIFT)	// 8K
#define PAGE_OFFSET  	(0x80000000UL)		// Kernel starts at 2G onwards
#endif

#define PAGE_MASK       (~(PAGE_SIZE-1))

#ifdef __KERNEL__

#include <asm/bug.h>

#define PHYS_SRAM_OFFSET 0x80000000

#define ARCH_PFN_OFFSET     (PAGE_OFFSET >> PAGE_SHIFT)
#define pfn_valid(pfn)      (((pfn) - (PAGE_OFFSET >> PAGE_SHIFT)) < max_mapnr)

/* Beware this looks cheap but it is pointer arithmatic
 * so becomes divide by sizeof which is not power of 2
 */
#define __pfn_to_page(pfn)  (mem_map + ((pfn) - ARCH_PFN_OFFSET))
#define __page_to_pfn(page) ((unsigned long)((page) - mem_map) + \
                                ARCH_PFN_OFFSET)

#define page_to_pfn __page_to_pfn
#define pfn_to_page __pfn_to_page

#ifndef __ASSEMBLY__

struct mm_struct;
struct vm_area_struct;
struct page;

extern void clear_page(void *page);
extern void copy_page(void *to, void *from);
extern void clear_user_page(void *addr, unsigned long vaddr, struct page *page);
extern void copy_user_page(void *vto, void *vfrom, unsigned long vaddr, struct page *to);

#define get_user_page(vaddr)        __get_free_page(GFP_KERNEL)
#define free_user_page(page, addr)  free_page(addr)

/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte_lo; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct page *pgtable_t;

#define pte_val(x)      ((x).pte_lo)
#define pmd_val(x)      ((x).pmd)
#define pgd_val(x)      ((x).pgd)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgd(x)        ((pgd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

/* Pure 2^n version of get_order */
extern __inline__ int get_order(unsigned long size)
{
    int order;

    size = (size-1) >> (PAGE_SHIFT-1);
    order = -1;
    do {
    size >>= 1;
        order++;
    } while (size);
    return order;
}

#define __pa(vaddr)  (((unsigned long)vaddr - PAGE_OFFSET) + PHYS_SRAM_OFFSET)
#define __va(paddr)  ((void *)(((unsigned long)(paddr) - PHYS_SRAM_OFFSET) + PAGE_OFFSET))

#define virt_to_page(kaddr) (mem_map + (__pa(kaddr) >> PAGE_SHIFT) - \
                                (PHYS_SRAM_OFFSET >> PAGE_SHIFT))

#define virt_addr_valid(kaddr)  pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

#define VALID_PAGE(page)    ((page - mem_map) < max_mapnr)

#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE | VM_EXEC | \
                    VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define PAGE_BUG(page) do { \
    BUG(); \
  } while (0)

#endif /* !__ASSEMBLY__ */

/* rtee: check if not using the page_address  macro helps */
#define WANT_PAGE_VIRTUAL   1

#endif  /* __KERNEL__ */

#endif /* __ASM_ARC_PAGE_H */
