/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/cacheflush.h> /* for flush_dcache_page_virt */
#include <linux/module.h>

/* page functions */

void clear_page(void *page)
{
	__asm__ __volatile__(
                 "lsr.f     lp_count,%1, 4\n"
                 "lpnz      1f\n"
			     "st.ab     0, [%0, 4]\n"
			     "st.ab     0, [%0, 4]\n"
			     "st.ab     0, [%0, 4]\n"
			     "st.ab     0, [%0, 4]\n"
                 "1:\n"
                 ::"r"(page), "r"(PAGE_SIZE)
			     :"memory","lp_count");
}

EXPORT_SYMBOL(clear_page);

void copy_page(void *to, void *from)
{
	unsigned long reg1, reg2, reg3, reg4;

	__asm__ __volatile__(
			     "lsr.f lp_count,%6,4\n"
                 "lpnz 1f\n"
			     "ld.ab  %0, [%5, 4]\n\t"
			     "ld.ab  %1, [%5, 4]\n\t"
			     "ld.ab  %2, [%5, 4]\n\t"
			     "ld.ab  %3, [%5, 4]\n\t"
			     "st.ab  %0, [%4, 4]\n\t"
			     "st.ab  %1, [%4, 4]\n\t"
			     "st.ab  %2, [%4, 4]\n\t"
			     "st.ab  %3, [%4, 4]\n\t"
                 "1:\n"
                 :"=&r"(reg1), "=&r"(reg2), "=&r"(reg3), "=&r"(reg4)
			     :"r"(to), "r"(from), "r"(PAGE_SIZE)
                 :"memory","lp_count"
	    );

}

/* Initialize the new pgd with invalid ptes */

void pgd_init(unsigned long page)
{
	int zero = 0;
	unsigned long dummy1, dummy2;
	__asm__ __volatile__("sub   %0, %0, 4\n\t"
			     "1:\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "sub.f %1, %1, 1\n\t"
			     "nop \n\t"
			     "bnz   1b\n\t":"=r"(dummy1), "=r"(dummy2)
			     :"r"(zero), "0"(page), "1"(USER_PTRS_PER_PGD / 8)
	    );

}

void clear_user_page(void *addr, unsigned long vaddr, struct page *page)
{
    clear_page(addr);

    if (arc_cache_meta.has_aliasing & ARC_DC_ALIASING)
        flush_dcache_page_virt((unsigned long *)page);

}

void copy_user_page(void *vto, void *vfrom, unsigned long vaddr, struct page *to)
{
    copy_page(vto,vfrom);

    if (arc_cache_meta.has_aliasing & ARC_DC_ALIASING)
        flush_dcache_page_virt((unsigned long*)vto);
}
