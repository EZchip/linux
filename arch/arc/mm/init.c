/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/initrd.h>
#endif
#ifdef CONFIG_BLOCK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/swap.h>
#include <linux/module.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/mmapcode.h>
#include <asm/sections.h>
#include <asm/arcregs.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
char empty_zero_page[PAGE_SIZE] __aligned(PAGE_SIZE);
EXPORT_SYMBOL(empty_zero_page);

/* Default tot mem from .config */
unsigned long end_mem = CONFIG_ARC_PLAT_SDRAM_SIZE + CONFIG_LINUX_LINK_BASE;

/* User can over-ride above with "mem=nnn[KkMm]" in cmdline */
static int __init setup_mem_sz(char *str)
{
	unsigned long sz_bytes;

	sz_bytes = memparse(str, NULL);
	end_mem = CONFIG_LINUX_LINK_BASE + (sz_bytes & PAGE_MASK);

	/* early console might not be setup yet - it will show up later */
	pr_info("\"mem=%s\": End mem set to 0x%lx\n", str, end_mem);

	return 0;
}
early_param("mem", setup_mem_sz);

void __init pagetable_init(void)
{
	pgd_init((unsigned long)swapper_pg_dir);
	pgd_init((unsigned long)swapper_pg_dir +
		 sizeof(pgd_t) * USER_PTRS_PER_PGD);
}

void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = { 0, 0 };

	pagetable_init();

	/* num_phypages has already been calc in setup_arch_memory( ) */
	zones_size[ZONE_NORMAL] = num_physpages;

	/*
	 * Must not use free_area_init() as that uses PAGE_OFFSET, which
	 * need not be the case when our kernel linked at non-default addr
	 * i.e. when CONFIG_LINUX_LINK_BASE != PAGE_OFFSET
	 */
#ifdef CONFIG_FLATMEM
	free_area_init_node(0,			/* node-id */
			    zones_size,		/* num pages per zone */
			    min_low_pfn,	/* first pfn of node */
			    NULL);		/* NO holes */
#else
#error "Fix !CONFIG_FLATMEM"
#endif
}

/*
 * mem_init - initializes memory
 *
 * Frees up bootmem
 * Calculates and displays memory available/used
 */
void __init mem_init(void)
{
	int codesize, datasize, initsize, reserved_pages, free_pages;
	int tmp;

	high_memory = (void *)end_mem;

	max_mapnr = num_physpages;

	totalram_pages = free_all_bootmem();

	/* count all reserved pages, including kernel code/data/mem_map.. */
	reserved_pages = 0;
	for (tmp = 0; tmp < max_mapnr; tmp++)
		if (PageReserved(mem_map + tmp))
			reserved_pages++;

	/* XXX: nr_free_pages() is equivalent */
	free_pages = max_mapnr - reserved_pages;

	/*
	 * kernel code/data is reserved and already shown explicitly,
	 * Show any other reservations (mem_map[ ] et al)
	 */
	reserved_pages -= (((unsigned int)_end - CONFIG_LINUX_LINK_BASE) >>
								PAGE_SHIFT);

	codesize = _etext - _text;
	datasize = _end - _etext;
	initsize = __init_end - __init_begin;

	pr_info("Memory Available: %dM / %uM "
		"(%dK code, %dK data, %dK init, %dK reserv)\n",
		PAGES_TO_MB(free_pages),
		TO_MB(CONFIG_ARC_PLAT_SDRAM_SIZE),
		TO_KB(codesize), TO_KB(datasize), TO_KB(initsize),
		PAGES_TO_KB(reserved_pages));
}

static void __init free_init_pages(const char *what, unsigned long begin,
				   unsigned long end)
{
 	unsigned long addr;

	printk(KERN_INFO "Freeing %s: %ldk [%lx] to [%lx]\n",
		what, TO_KB(end - begin), begin, end);

	/* need to check that the page we free is not a partial page */
	for (addr = begin; addr + PAGE_SIZE <= end; addr += PAGE_SIZE) {
 		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
 		free_page(addr);
 		totalram_pages++;
 	}
}

/*
 * free_initmem: Free all the __init memory.
 */
void __init_refok free_initmem(void)
{
	free_init_pages("unused kernel memory", (unsigned long)__init_begin,
			(unsigned long)__init_end);

	mmapcode_space_init();
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
	free_init_pages("initrd memory", start, end);
}

void __init reserve_initrd_mem(void)
{
	unsigned long size = initrd_end - initrd_start;
	
	if (size > 0)
		reserve_bootmem(__pa(initrd_start), size, BOOTMEM_DEFAULT);
}

static int __init rd_start_early(char *p)
{
	unsigned long start = memparse(p, &p);

	initrd_start = start;
	initrd_end += start;
	return 0;
}
early_param("rd_start", rd_start_early);

static int __init rd_size_early(char *p)
{
	initrd_end += memparse(p, &p);
	return 0;
}
early_param("rd_size", rd_size_early);

#endif

/*
 * Count the pages we have and Setup bootmem allocator
 */
void __init setup_arch_memory(void)
{
	int bootmap_sz;
	unsigned int first_free_pfn;
	unsigned long kernel_img_end, alloc_start;

	init_mm.start_code = (unsigned long)_text;
	init_mm.end_code = (unsigned long)_etext;
	init_mm.end_data = (unsigned long)_edata;
	init_mm.brk = (unsigned long)_end;

	/* _end needs to be page aligned */
	kernel_img_end = (unsigned long)_end;
	BUG_ON(kernel_img_end & ~PAGE_MASK);

	/* First free page beyond kernel image */
	first_free_pfn = PFN_DOWN(kernel_img_end);

	/* first page of system - kernel .vector starts here */
	min_low_pfn = PFN_DOWN(CONFIG_LINUX_LINK_BASE);

	/*
	 * Last usable page of low mem (no HIGH_MEM yet for ARC port)
	 * -must be BASE + SIZE
	 */
	max_low_pfn = max_pfn = PFN_DOWN(end_mem);

	num_physpages = max_low_pfn - min_low_pfn;

	/* setup bootmem allocator */
	bootmap_sz = init_bootmem_node(NODE_DATA(0),
				       first_free_pfn,/* bitmap start */
				       min_low_pfn,   /* First pg to track */
				       max_low_pfn);  /* Last pg to track */

	/*
	 * init_bootmem above marks all tracked Page-frames as inuse "allocated"
	 * This includes pages occupied by kernel's elf segments.
	 * Beyond that, excluding bootmem bitmap itself, mark the rest of
	 * free-mem as "allocatable"
	 */
	alloc_start = kernel_img_end + bootmap_sz;
	free_bootmem(alloc_start, end_mem - alloc_start);

#ifdef CONFIG_BLK_DEV_INITRD
	reserve_initrd_mem();
#endif
}
