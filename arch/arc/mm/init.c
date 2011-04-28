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
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) ARC International
 * Authors: Rahul Trivedi
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#ifdef CONFIG_BLOCK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/swap.h>

#include <asm/page.h>
#include <asm/pgalloc.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __attribute__ ((aligned(PAGE_SIZE)));
char empty_zero_page[PAGE_SIZE] __attribute__ ((aligned(PAGE_SIZE)));

/* static unsigned long totalram_pages = 0; */
extern char _etext, _text, _edata, _end, _init_end, _init_begin;

void __init pagetable_init(void)
{
	pgd_init((unsigned long)swapper_pg_dir);
	pgd_init((unsigned long)swapper_pg_dir +
		 sizeof(pgd_t) * USER_PTRS_PER_PGD);
}

void __init paging_init(void)
{
	/* Sameer: MAX_NR_ZONES is just 2 for us */
	unsigned long zones_size[MAX_NR_ZONES] = { 0, 0 };
	extern unsigned long end_mem;

	pagetable_init();

#if 0
	zones_size[ZONE_DMA] = max_low_pfn;

	free_area_init(zones_size);
#else
	zones_size[ZONE_NORMAL] =
	    ((unsigned long)(end_mem) - PHYS_SRAM_OFFSET) >> PAGE_SHIFT;

#ifdef  CONFIG_FLATMEM

	free_area_init_node(0, zones_size, __pa(PAGE_OFFSET) >> PAGE_SHIFT, NULL);
#endif
#endif
}

void __init mem_init(void)
{
	int codesize, datasize, initsize, reserved_pages;
	int tmp;

	high_memory =
	    (void *)__va((max_low_pfn) * PAGE_SIZE);

	max_mapnr = num_physpages;

	totalram_pages = free_all_bootmem();

	reserved_pages = 0;
	for (tmp = 0; tmp < num_physpages; tmp++)
		if (PageReserved(mem_map + tmp))
			reserved_pages++;

	codesize = (unsigned long)&_etext - (unsigned long)&_text;
	datasize = (unsigned long)&_end - (unsigned long)&_etext;
	initsize = (unsigned long)&_init_end - (unsigned long)&_init_begin;

	printk(KERN_NOTICE
	       "Memory: %luKB available (%dK code,%dK data, %dK init)\n",
	       (unsigned long)nr_free_pages() << (PAGE_SHIFT - 10),
	       codesize >> 10, datasize >> 10, initsize >> 10);
}

void free_initmem(void)
{
	extern char _init_begin, _init_end;
	unsigned long addr;

	addr = (unsigned long)(&_init_begin);
	for (; addr < (unsigned long)(&_init_end); addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		/* Sameer: may be arch-specific code is not supposed to use
		   this. */
		/* set_page_count(virt_to_page(addr), 1); */
		free_page(addr);
		totalram_pages++;
	}
	printk(KERN_INFO "Freeing unused kernel memory: %luk freed [%p] TO [%p]\n",
	       (&_init_end - &_init_begin) >> 10, &_init_begin, &_init_end);
}

void show_mem(void)
{
	int i, free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0;

	printk("Mem-info:\n");
	show_free_areas();
	printk("Free swap:       %6dkB\n",
	       (int)(nr_swap_pages << (PAGE_SHIFT - 10)));
	i = max_mapnr;
	while (i-- > 0) {
		total++;
		if (PageReserved(mem_map + i))
			reserved++;
		else if (PageSwapCache(mem_map + i))
			cached++;
		else if (!page_count(mem_map + i))
			free++;
		else
			shared += page_count(mem_map + i) - 1;
	}
	printk("%d pages of RAM\n", total);
	printk("%d reserved pages\n", reserved);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
	printk("%d free pages\n", free);
	/* Sameer: Seems to be obsolete. We need a look here too */
	/*      show_buffers(); */
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{

	int pages = 0;
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		/* Sameer: may be arch-specific code is not supposed to use
		   this. */
		/* set_page_count(virt_to_page(start), 1); */
		free_page(start);
		totalram_pages++;
		pages++;
	}
	printk("Freeing initrd memory: %luk freed\n",
	       (pages * PAGE_SIZE) >> 10);

}
#endif
