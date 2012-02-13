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
 *  linux/include/asm-arc/pgalloc.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Autors : Amit Bhor, Sameer Dhavale
 * PGD, PMD, PTE allocation and freeing routines, taken from mips
 */

#ifndef _ASM_ARC_PGALLOC_H
#define _ASM_ARC_PGALLOC_H

#include <linux/highmem.h>

static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	pmd_set(pmd, (pte_t *)pte);
}

static inline void
pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t ptep)
{
    pmd_set(pmd, (pte_t *)(unsigned long)page_address(ptep));
}

#define PTE_ORDER 0

extern __inline__ pgd_t *get_pgd_slow(void)
{
    int num, num2;
    pgd_t *ret = (pgd_t *)__get_free_page(GFP_KERNEL);

    if (ret) {
        num = USER_PTRS_PER_PGD + USER_KERNEL_GUTTER/PGDIR_SIZE;
        memset(ret, 0, num * sizeof(pgd_t));

        num2 = VMALLOC_SIZE/PGDIR_SIZE;
        memcpy(ret + num, swapper_pg_dir + num, num2 * sizeof(pgd_t));

        memset(ret + num + num2, 0,
               (PTRS_PER_PGD - num - num2) * sizeof(pgd_t));

    }
    return ret;
}

extern __inline__ void free_pgd_slow(pgd_t *pgd)
{
        free_page((unsigned long)pgd);
}

static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
    pte_t *pte;

    pte = (pte_t *) __get_free_pages(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO, PTE_ORDER);

    return pte;
}

static inline pgtable_t
pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
    pgtable_t pte_pg;

    pte_pg = alloc_pages(GFP_KERNEL | __GFP_REPEAT, PTE_ORDER);
    if (pte_pg)
    {
        void *pte_phy = page_address(pte_pg);

         __asm__ __volatile__(
                  "mov     lp_count,%1\n"
                  "lp      1f\n"
                  "st.ab     0, [%0, 4]\n"
                  "st.ab     0, [%0, 4]\n"
                  "1:\n"
                  :"+r"(pte_phy)
                  :"r"(PTRS_PER_PTE/2) // 2 for 2 insn above
                  :"lp_count");
    }

    return pte_pg;
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
    free_pages((unsigned long)pte, PTE_ORDER);  // takes phy addr
}

static inline void pte_free(struct mm_struct *mm, pgtable_t ptep)
{
    __free_page(ptep);  //  takes struct page*
}


#define pgd_free(mm, pgd)      free_pgd_slow(pgd)
#define pgd_alloc(mm)          get_pgd_slow()

#define __pte_free_tlb(tlb,pte)     tlb_remove_page((tlb),(pte))

extern void pgd_init(unsigned long page);

#define check_pgt_cache()   do { } while (0)

#define pmd_pgtable(pmd) pmd_page(pmd)

#endif  /* _ASM_ARC_PGALLOC_H */
