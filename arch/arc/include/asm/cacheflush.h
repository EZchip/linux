/*************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 *  vineetg: April 2008
 *      -Added a critical CacheLine flush to copy_to_user_page( ) which
 *          was causing gdbserver to not setup breakpoints consistently
 *
 ************************************************************************/

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
 *  linux/include/asm-arc/cacheflush.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Sameer Dhavale
 */

#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#include <linux/mm.h>

/* Sameer: Getting these definitions from MIPS as
           seemed simpler for a while */
#define flush_dcache_mmap_lock(mapping)     do { } while (0)
#define flush_dcache_mmap_unlock(mapping)   do { } while (0)
#define flush_cache_vmap(start, end)        flush_cache_all()
#define flush_cache_vunmap(start, end)      flush_cache_all()

#ifdef CONFIG_ARC700_CACHE

#ifdef CONFIG_SMP
#error "Caching not yet supported in SMP"
#error "remove CONFIG_ARC700_USE_ICACHE and CONFIG_ARC700_USE_DCACHE"
#endif


extern void flush_cache_all(void);
extern void __flush_cache_all(void);
extern void flush_cache_mm(struct mm_struct *mm);

extern void flush_cache_range(struct vm_area_struct *vma, unsigned long start,unsigned long end);
extern void flush_cache_page(struct vm_area_struct *vma, unsigned long user_addr, unsigned long page);

#else

#define flush_cache_all()                       do { } while (0)
#define flush_cache_mm(mm)                      do { } while (0)
#define flush_cache_range(mm,start,end)         do { } while (0)
#define flush_cache_page(vma,user_addr,page)    do { } while (0)


#endif /* CONFIG_ARC_CACHE */

// TODO-vineetg : added after 2.6.19 need to look at this
#define flush_cache_dup_mm(mm)  flush_cache_mm(mm)

#ifdef CONFIG_ARC700_USE_ICACHE
extern void flush_icache_all(void);
extern void flush_icache_range(unsigned long start, unsigned long end);
extern void flush_icache_page(struct vm_area_struct *vma,struct page *page);

#else

#define flush_icache_all()                      do { } while (0)
#define flush_icache_range(start,end)           do { } while (0)
#define flush_icache_page(vma,page)             do { } while (0)

#endif /*CONFIG_ARC700_USE_ICACHE*/

#ifdef CONFIG_ARC700_USE_DCACHE
extern void flush_dcache_page(struct page *page);
extern void flush_dcache_page_virt(unsigned long *page);
extern void flush_dcache_range(unsigned long start,unsigned long end);

extern void flush_dcache_all(void);
extern void inv_dcache_all(void);
extern void flush_and_inv_dcache_range(unsigned long start, unsigned long end);
extern void inv_dcache_range(unsigned long start, unsigned long end);
void flush_and_inv_dcache_all(void);

#define dma_cache_wback_inv(start,size) flush_and_inv_dcache_range(start, start + size)
#define dma_cache_wback(start,size)     flush_dcache_range(start, start + size)
#define dma_cache_inv(start,size)       inv_dcache_range(start, start + size)

#else

#define flush_dcache_range(start,end)           do { } while (0)
#define flush_dcache_page(page)                 do { } while (0)
#define flush_dcache_page_virt(page)            do { } while (0)
#define flush_dcache_all(start,size)            do { } while (0)
#define inv_dcache_all()                        do { } while (0)
#define inv_dcache_range(start,size)            do { } while (0)
#define flush_and_inv_dcache_all()              do { } while (0)
#define flush_and_inv_dcache_range(start, end)  do { } while (0)

#define dma_cache_wback_inv(start,size)         do { } while (0)
#define dma_cache_wback(start,size)             do { } while (0)
#define dma_cache_inv(start,size)               do { } while (0)
#endif /*CONFIG_ARC700_USE_DCACHE*/

/*
 * Copy user data from/to a page which is mapped into a different
 * processes address space.  Really, we want to allow our "user
 * space" model to handle this.
 */
#define copy_to_user_page(vma, page, vaddr, dst, src, len)  \
    do {                                                    \
        memcpy(dst, src, len);                              \
        if (vma->vm_flags & VM_EXEC )   {                   \
            flush_icache_range((unsigned long)(dst),        \
                               (unsigned long)(dst+len)); \
        }                                                   \
    } while (0)


#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
    do {                            \
        memcpy(dst, src, len);              \
    } while (0)


#include <asm/arcregs.h>
extern struct cpuinfo_arc cpuinfo_arc700[];

#endif
