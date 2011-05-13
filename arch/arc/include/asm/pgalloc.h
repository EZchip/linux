/******************************************************************************
 * Copyright ARC International (www.arc.com) 2010-2011
 *
 * vineetg: May 2011
 *  -Variable pg-sz means that Page Tables could be variable sized themselves
 *    So calculate it based on addr traversal split [pgd-bits:pte-bits:xxx]
 *    in __get_order_pgd( ) and __get_order_pte( ).
 *    Since these deal with constants, they are optimised away by gcc.
 *
 * vineetg: Nov 2010
 *  -Added pgtable ctor/dtor used for pgtable mem accounting
 *
 * vineetg: April 2010
 *  -Switched pgtable_t from being struct page * to unsigned long
 *      =Needed so that Page Table allocator (pte_alloc_one) is not forced to
 *       to deal with struct page. Thay way in future we can make it allocate
 *       multiple PG Tbls in one Page Frame
 *      =sweet side effect is avoiding calls to ugly page_address( ) from the
 *       pg-tlb allocator sub-sys (pte_alloc_one, ptr_free, pmd_populate
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
#include <linux/log2.h>

/* @sz is bytes, but gauranteed to be multiple of 4
 * Similarly @ptr is alo word aligned
 */
static void inline memset_aligned(void *ptr, unsigned int sz)
{
    void *tmp = ptr;

    __asm__ __volatile__(
                  "mov     lp_count,%1\n"
                  "lp      1f\n"
                  "st.ab     0, [%0, 4]\n"
                  "st.ab     0, [%0, 4]\n"
                  "1:\n"
                  :"+r"(tmp)
                  :"ir"(sz/4/2) // 4: bytes to word
                                // 2: instances of st.ab in loop
                  :"lp_count");

}

static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
    pmd_set(pmd, pte);
}

static inline void
pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t ptep)
{
    pmd_set(pmd, (pte_t *)ptep);
}

static inline int __get_order_pgd(void)
{
    const int num_pgs = (PTRS_PER_PGD * 4)/PAGE_SIZE;

    if (num_pgs)
        return order_base_2(num_pgs);

    return 0;  /* 1 Page */
}

static inline pgd_t *get_pgd_slow(void)
{
    int num, num2;
    pgd_t *ret = (pgd_t *)__get_free_pages(GFP_KERNEL, __get_order_pgd());

    if (ret) {
        num = USER_PTRS_PER_PGD + USER_KERNEL_GUTTER/PGDIR_SIZE;
        memset_aligned(ret, num * sizeof(pgd_t));

        num2 = VMALLOC_SIZE/PGDIR_SIZE;
        memcpy(ret + num, swapper_pg_dir + num, num2 * sizeof(pgd_t));

        memset_aligned(ret + num + num2,
               (PTRS_PER_PGD - num - num2) * sizeof(pgd_t));

    }
    return ret;
}

static inline void free_pgd_slow(pgd_t *pgd)
{
        free_pages((unsigned long)pgd, __get_order_pgd());
}

#define pgd_free(mm, pgd)      free_pgd_slow(pgd)
#define pgd_alloc(mm)          get_pgd_slow()

static inline int __get_order_pte(void)
{
    /* SASID requires PTE to be two words - thus size of pg tbl is doubled */
#ifdef CONFIG_ARC_MMU_SASID
    const int multiplier = 2;
#else
    const int multiplier = 1;
#endif

    const int num_pgs = (PTRS_PER_PTE * 4 * multiplier)/PAGE_SIZE;

    if (num_pgs)
        return order_base_2(num_pgs);

    return 0;  /* 1 Page */
}


static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
    pte_t *pte;

    pte = (pte_t *) __get_free_pages(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO,
									__get_order_pte());

    return pte;
}

static inline pgtable_t
pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
    pgtable_t pte_pg;

    pte_pg = __get_free_pages(GFP_KERNEL | __GFP_REPEAT, __get_order_pte());
    if (pte_pg)
    {
		memset_aligned((void *)pte_pg, PTRS_PER_PTE * 4);
		pgtable_page_ctor(virt_to_page(pte_pg));
    }

    return pte_pg;
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
    free_pages((unsigned long)pte, __get_order_pte());  // takes phy addr
}

static inline void pte_free(struct mm_struct *mm, pgtable_t ptep)
{
	pgtable_page_dtor(virt_to_page(ptep));
    free_pages(ptep, __get_order_pte());
}


#define __pte_free_tlb(tlb, pte, addr)  tlb_remove_page(tlb, virt_to_page(pte))

extern void pgd_init(unsigned long page);

#define check_pgt_cache()   do { } while (0)

#define pmd_pgtable(pmd) pmd_page_vaddr(pmd)

#endif  /* _ASM_ARC_PGALLOC_H */
