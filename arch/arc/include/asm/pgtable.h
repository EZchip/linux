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
 *  linux/include/asm-arc/pgtable.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_PGTABLE_H
#define _ASM_ARC_PGTABLE_H

#include <asm/page.h>
#include <asm/mmu.h>
#include <asm-generic/4level-fixup.h>
/* The linux MM assumes a 3 level page table mechanism. The ARC 700 MMU has
 * a software managed TLB so it can support any levels of paging. We use a 3
 * level page table which folds into a 2 level page table (similar to i386)
 * since it seems to be more memory efficient this way.
 */

/* PMD_SHIFT determines the size of the area that can be mapped by a second
 * level page table. In memory ARC700 linux PTE's are 4 bytes wide as follows :
 *
 *	31            13|12      8        5      3      0
 * 	 -------------------------------------------------
 * 	|       PFN     |M|V|L|G|Rk|Wk|Ek|Ru|Wu|Eu|Fc|A|P |
 *	 -------------------------------------------------
 *
 *	Rk, Ek and Wk are not used by linux. The kernel has all permissions
 *	hence they are each set to 1.
 *	M -> Modified(dirty) , A -> Accessed , P -> Present are implemented
 *	in software.
 *	V->Valid, L->Locked, G->Global are kept here and copied into TLBPD0
 *	when updating the TLB.
 */
#define __PAGETABLE_PMD_FOLDED 1
#define PMD_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT -2 )) /* 24 , PAGE_SHIFT 13 */
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))

/* PGDIR_SHIFT determines the size of the area mapped by a third level page
 * table entry. Since we fold into a 2 level structure this is the same as
 * PMD_SHIFT.
 */
#define PGDIR_SHIFT	PMD_SHIFT
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))

/* Entries per directory level. We use a 2 level page table structure so we
 * dont have a physical PMD. On A700 pointers are 4 bytes wide so divide
 * page size by 4
 */
#define	PTRS_PER_PTE	2048
#define	PTRS_PER_PMD	1
#define	PTRS_PER_PGD	256

/* Number of entries a user land program use .
 * TASK_SIZE is the maximum Virtual address that can be used by a userland
 * program.
 */
#define	USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

/* Sameer: Need to recheck this value for ARC. */
#define FIRST_USER_ADDRESS      0

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

#ifndef __ASSEMBLY__

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

/* the zero page used for uninitialized and anonymous pages */
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))

#define pte_unmap(pte)		do { } while (0)
#define pte_unmap_nested(pte)		do { } while (0)

#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)


static inline void flush_tlb_pgtables(struct mm_struct *mm,
				      unsigned long start, unsigned long end)
{
  /* ARC doesn't keep page table caches in TLB */
}

/* Certain architectures need to do special things when pte's
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
/*
 * (pmds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = pmdval)
#define set_pgd(pgdptr, pgdval) (*(pgdptr) = pgdval)

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
/* #define pmd_phys(pmd)		(pmd_val(pmd) - PAGE_OFFSET) */
/* #define pmd_page(pmd)		(pfn_to_page(pmd_phys(pmd) >> PAGE_SHIFT)) */
#define pmd_page(pmd) virt_to_page(__va(pmd_val(pmd) & PAGE_MASK))

#define pmd_page_vaddr(pmd)	(pmd_val(pmd) & PAGE_MASK)

/* 	{ return (pmd_val(pmd) & PAGE_MASK); } */
static inline void pmd_set(pmd_t * pmdp, pte_t * ptep)
	{ pmd_val(*pmdp) = (_PAGE_TABLE | (unsigned long) ptep); }

#define pte_none(x)	(!pte_val(x))
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
/* #define pte_clear(xp)	do { pte_val(*(xp)) = 0; } while (0) */

#define pte_clear(mm,addr,ptep)	set_pte_at((mm),(addr),(ptep), __pte(0))

#define pmd_none(x)	(!pmd_val(x))
/* by removing the _PAGE__KERNEL bit from the comparision, the same pmd_bad
 * works for both _PAGE_TABLE and _KERNPG_TABLE pmd entries.
 */
#define	pmd_bad(x)	((pmd_val(x) & ~PAGE_MASK) != _PAGE_TABLE)
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define pmd_clear(xp)	do { pmd_val(*(xp)) = 0; } while (0)

/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }
static inline void pgd_clear(pgd_t * pgdp)	{ }

#define pte_page(x) (mem_map + (unsigned long)(((pte_val(x)-PAGE_OFFSET) >> PAGE_SHIFT)))
/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)	{ return pte_val(pte) & _PAGE_READ; }
static inline int pte_write(pte_t pte)	{ return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_dirty(pte_t pte)	{ return pte_val(pte) & _PAGE_MODIFIED; }
static inline int pte_young(pte_t pte)	{ return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_special(pte_t pte) { return 0;}

/* Sameer: A new macro for 2.6 We need a deeper look here. */
static inline int pte_file(pte_t pte)	{ return pte_val(pte) & _PAGE_FILE; }

/* Sameer: Temporary def */
#define PTE_FILE_MAX_BITS	30

/* Sameer: These are useful for non-linear memory mapped files. I am
           postponing the implementation till it actually becomes a blocker.
            We need arch-specific implementation here. Which I am not doing
	    right now. */

#define pgoff_to_pte(x)         __pte(x)

#define pte_to_pgoff(x)		(pte_val(x) >> 2)

#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)

#define pfn_pte(pfn,prot)	(__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot)))

#define __pte_offset(address)						\
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define __pte_index(addr)	(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define pte_offset_kernel(dir,addr)	((pte_t *)pmd_page_vaddr(*(dir)) + __pte_index(addr))
#define pte_offset_map(dir,addr)	((pte_t *)pmd_page_vaddr(*(dir)) + __pte_index(addr))
#define pte_offset_map_nested(dir,addr)	((pte_t *)pmd_page_vaddr(*(dir)) + __pte_index(addr))

/*  Sameer: MIPS-clone. Defining a macro instead of function in old 2.4 days */
#define pte_offset(dir, address)					\
	((pte_t *) (pmd_page_vaddr(*dir)) + __pte_offset(address))

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_WRITE );
	return pte;
}

static inline pte_t pte_rdprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_READ );
	return pte;
}

static inline pte_t pte_exprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_EXECUTE);
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_MODIFIED);
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	return pte;
}

static inline pte_t pte_mkread(pte_t pte)
{
	pte_val(pte) |= _PAGE_READ;
	return pte;
}

static inline pte_t pte_mkexec(pte_t pte)
{
	pte_val(pte) |= _PAGE_EXECUTE;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_MODIFIED;
	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	return pte;
}

static inline pte_t pte_mkspecial(pte_t pte)
{
    return pte;
}

/* Macro to mark a page protection as uncacheable */
#define pgprot_noncached pgprot_noncached

static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot = (prot & ~_PAGE_CACHEABLE);

	return __pgprot(prot);
}

#if 0
#define __mk_pte(page_nr, pgprot) __pte(((page_nr) << PAGE_SHIFT) | 	\
						pgprot_val(pgprot))
#define mk_pte(page, pgprot) __mk_pte((page) - mem_map, (pgprot))
#else

#define mk_pte(page, pgprot)  \
 ({ \
      pte_t pte;  \
      pte_val(pte) = __pa(page_address(page)) + pgprot_val(pgprot); \
      pte; \
 })
#endif

static inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{
	return __pte(physpage | pgprot_val(pgprot));
}

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

/* to find an entry in a kernel page-table-directory.
 * All kernel related VM pages are in init's mm.
 */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* to find an entry in a page-table-directory */
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)

#define pgd_offset(mm, addr)	(((mm)->pgd)+pgd_index(addr))

/* Macro to quickly access the PGD entry, utlising the fact that some
 * arch may cache the pointer to Page Directory of "current" task
 * in a MMU register
 *
 * Thus task->mm->pgd (3 pointer dereferences, cache misses etc simply
 * becomes read a register
 *
 * ********CAUTION*******:
 * Kernel code might be dealing with some mm_struct of NON "current"
 * Thus use this macro only when you are certain that "current" is current
 * e.g. when dealing with signal frame setup code etc
 */
#define pgd_offset_fast(mm, addr)\
({ \
    pgd_t *pgd_base = (pgd_t *) read_new_aux_reg(ARC_REG_SCRATCH_DATA0);  \
    pgd_base + pgd_index(addr); \
})

/* Find an entry in the second-level page table */
static inline pmd_t *pmd_offset(pud_t *dir, unsigned long address)
{
	return (pmd_t *) dir;
}

/* /\* Find an entry in the third-level page table.. *\/ */
/* static inline pte_t *pte_offset(pmd_t * dir, unsigned long address) */
/* { */
/* 	return (pte_t *) (pmd_page(*dir)) + */
/* 	       ((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1)); */
/* } */

extern int do_check_pgt_cache(int, int);
extern void paging_init(void);

extern char empty_zero_page[PAGE_SIZE];
extern pgd_t swapper_pg_dir[]__attribute__((aligned(PAGE_SIZE)));

void update_mmu_cache(struct vm_area_struct *vma,
				 unsigned long address, pte_t pte);

/* Linux reserves _PAGE_PRESENT of the pte for swap implementation.
 * Our _PAGE_PRESENT is in bit 0 so we can use 5 bits 1-6 for swap type
 * and top 24 bits for swap offset ( Max 64 GB  swap area )
 */
#define __swp_type(x)		(((x).val >> 1) & 0x1f)
#define __swp_offset(x)		((x).val >> 8)
#define __swp_entry(type,offset)	((swp_entry_t) { ((type) << 1) | ((offset) << 8) })

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#include <asm-generic/pgtable.h>

/*
 * remap a physical page `pfn' of size `size' with page protection `prot'
 * into virtual address `from'
 */
#define io_remap_pfn_range(vma,from,pfn,size,prot) \
		remap_pfn_range(vma, from, pfn, size, prot)

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()   do { } while (0)

#endif /* __ASSEMBLY__ */
#endif /* _ARM_ARC_PGTABLE_H */
