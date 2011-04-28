/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * Vineetg: Mar 2009
 *  -D-Cache Invalidate mode set to INV ONLY as that is more common than
 *      FLUSH BEFORE INV (not called at all)
 *
 * Vineetg: Jan 2009
 *  -Cache Line flush routines used to flush an extra line beyond end addr
 *   because check was while (end >= start) instead of (end > start)
 *     =Some call sites had to work around by doing -1, -4 etc to end param
 *     ==Some callers didnt care. This was spec bad in case of INV routines
 *       which would discard valid data (cause of the horrible ext2 bug
 *       in ARC IDE driver)
 *
 * Vineetg: Oct 3rd 2008:
 *  -Got rid of un-necessary globals such as Cache line size,
 *      checks if cache enabled in entry of each cache routine etc
 *  -Cache Meta data (sz, alising etc) saved in a seperate structure rather than
 *      in the big cpu info struct
 *
 * vineetg: June 11th 2008:
 *  -Fixed flush_icache_range( )
 *   + As an API it is called by kernel while doing insmod after copying module
 *     code from user space to kernel space in load_module( ).
 *     Later kernel executes module out of this COPIED OVER kernel memory.
 *     Since ARC700 caches are not coherent (I$ doesnt snoop D$) both need
 *     to be flushed in flush_icache_range() which it was not doing.
 *   + load_module( ) passes vmalloc addr (Kernel Virtual Addr) to the API,
 *     however ARC cache maintenance OPs require PHY addr. Thus need to do
 *     vmalloc_to_phy.
 *   + Also added optimisation there, that for range > PAGE SIZE we flush the
 *     entire cache in one shot rather than line by line. For e.g. a module
 *     with Code sz 600k, old code flushed 600k worth of cache (line-by-line),
 *     while cache is only 16 or 32k.
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
 *  linux/arch/arc/mm/cache_arc700.c
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Rahul Trivedi
 * I-Cache and D-cache control functionality.
 * Ashwin Chaugule <ashwin.chaugule@codito.com>
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/page.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>

//#define ARC_CACHE_DEBUG

/*  Some Bit values */
#define DC_FLUSH_STATUS_BIT             0x100
#define INV_MODE_FLUSH                  0x40
#define CACHE_DISABLE_BIT               0x01

extern struct cpuinfo_arc cpuinfo_arc700[];

/* [size,virtual-aliasing] Info for I and D caches
 * Performance critical info seperated from cpuinfo_arc700[ ]
 * so that it sits in it's own cache line
 */
struct arc_cache arc_cache_meta;

/* Cache Build Config Reg encodes size in 4 bits as follows:
 *  Binary 0100 =>  8k
 *  Binary 0101 => 16k
 *  Binary 0110 => 32k
 *  Binary 0111 => 64k
 *  True for Both I$ and D$
 */
#define CALC_CACHE_SZ(hw_val)   (0x200 << hw_val)

char * arc_cache_mumbojumbo(int cpu_id, char *buf)
{
    int num = 0;

    num += sprintf(buf+num,"Detected I-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
            ARC_ICACHE_WAY_NUM, ARC_ICACHE_LINE_LEN, arc_cache_meta.i_sz>>10);

#ifdef CONFIG_ARC700_USE_ICACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    num += sprintf(buf+num,"Detected D-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
             ARC_DCACHE_WAY_NUM, ARC_DCACHE_LINE_LEN, arc_cache_meta.d_sz>>10);

#ifdef  CONFIG_ARC700_USE_DCACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    return buf;
}

void __init a7_probe_cache(void)
{
    unsigned int temp, sz;
    struct bcr_cache *p_i_bcr, *p_d_bcr;  /* Build Config Reg */

    /* Plug in the seperated Cache meta data struct to
     * the top level structure which contains info abt
     * other components such as MMU, EXTENSIONs etc
     */
    cpuinfo_arc700[smp_processor_id()].cache = &arc_cache_meta;

    ARC_shmlba = max(ARC_shmlba, (unsigned int)PAGE_SIZE);

    /****************** I-cache Probing *******************/

    /* load the icache build register */
    temp = read_new_aux_reg(ARC_REG_I_CACHE_BUILD_REG);
    p_i_bcr = (struct bcr_cache *)&temp;

#ifdef CONFIG_ARC700_USE_ICACHE
    /* Confirm some of I-cache params which Linux assumes */
    if ( ( p_i_bcr->type != 0x3 ) ||      /* 2 way set assoc */
         ( p_i_bcr->line_len != 0x2 ) )   /* 32 byte line length */
        goto sw_hw_mismatch;
#endif

    /* Convert encoded size to real value */
    sz = arc_cache_meta.i_sz = CALC_CACHE_SZ(p_i_bcr->sz);

    /* check whether Icache way size greater than PAGE_SIZE as it
     * cause Aliasing Issues and requires special handling
     */
    if ( (sz / ARC_ICACHE_WAY_NUM) > PAGE_SIZE) {
        arc_cache_meta.has_aliasing |= ARC_IC_ALIASING;
        ARC_shmlba = max(ARC_shmlba, sz / ARC_ICACHE_WAY_NUM);
    }

    /* load the icache Control register */
    temp = read_new_aux_reg(ARC_REG_IC_CTRL);

#ifdef CONFIG_ARC700_USE_ICACHE
    write_new_aux_reg(ARC_REG_IC_CTRL, temp & (~CACHE_DISABLE_BIT));
#else
    write_new_aux_reg(ARC_REG_IC_CTRL, (temp | CACHE_DISABLE_BIT));
#endif

    /****************** D-cache Probing *******************/

    /* load the dcache build register */
#ifdef CONFIG_ARC700_USE_DCACHE
    temp = read_new_aux_reg(ARC_REG_D_CACHE_BUILD_REG);
    p_d_bcr = (struct bcr_cache *)&temp;

    /* Confirm some of D-cache params which Linux assumes */
    if ( ( p_d_bcr->type != 0x2 ) ||      /* 4 way set assoc */
         ( p_d_bcr->line_len != 0x01 ) )  /* 32 byte line length */
       goto sw_hw_mismatch;

    /* Convert encoded size to real value */
    sz = arc_cache_meta.d_sz = CALC_CACHE_SZ(p_d_bcr->sz);

    /* check whether dcache way size greater than PAGE_SIZE */
    if ((sz / ARC_DCACHE_WAY_NUM) > PAGE_SIZE) {
        arc_cache_meta.has_aliasing |= ARC_DC_ALIASING;
        ARC_shmlba = max(ARC_shmlba, sz / ARC_DCACHE_WAY_NUM);
    }

    /* load the dcache Control register */
    temp = read_new_aux_reg(ARC_REG_DC_CTRL);

    /* Set the default Invalidate Mode to "simpy discard dirty lines"
     *  as this is more frequent then flush before invalidate
     * Ofcourse we toggle this default behviour when desired
     */
    temp &= ~INV_MODE_FLUSH;
#endif

#ifdef  CONFIG_ARC700_USE_DCACHE
    /* Enable D-Cache: Clear Bit 0 */
    write_new_aux_reg(ARC_REG_DC_CTRL, temp & (~CACHE_DISABLE_BIT));
#else
    /* Flush D cache */
    write_new_aux_reg(ARC_REG_DC_FLSH, 0x1);
    /* Disable D cache */
    write_new_aux_reg(ARC_REG_DC_CTRL, temp | CACHE_DISABLE_BIT);
#endif

    {
        char str[512];
        printk(arc_cache_mumbojumbo(0, str));
    }
    return;

sw_hw_mismatch:
    panic("Cache H/W doesn't match kernel Config");
}

#ifdef CONFIG_ARC700_USE_DCACHE

inline void flush_and_inv_dcache_all(void)
{
    unsigned long flags, dc_ctrl;

    local_irq_save(flags);

    /* Set the Invalidate mode to FLUSH BEFORE INV */
    dc_ctrl = read_new_aux_reg(ARC_REG_DC_CTRL);
    write_new_aux_reg(ARC_REG_DC_CTRL, dc_ctrl | INV_MODE_FLUSH);

    /* Invoke Cache INV CMD */
    write_new_aux_reg(ARC_REG_DC_IVDC, 0x1);

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & DC_FLUSH_STATUS_BIT) ;

    /* Set the Invalidate mode back to INV ONLY */
	write_new_aux_reg(ARC_REG_DC_CTRL, dc_ctrl & ~INV_MODE_FLUSH);

    local_irq_restore(flags);
}

EXPORT_SYMBOL(flush_and_inv_dcache_all);

/*
 * start, end - kernel virt addrs
 */

void flush_dcache_range(unsigned long start, unsigned long end)
{
    unsigned long flags;

    start &= DCACHE_LINE_MASK;
    local_irq_save(flags);

    while (end > start) {
        write_new_aux_reg(ARC_REG_DC_FLDL, start);
        start = start + ARC_DCACHE_LINE_LEN;
    }

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & DC_FLUSH_STATUS_BIT) ;

    local_irq_restore(flags);
}

void flush_dcache_page(struct page *page)
{
    flush_dcache_range((unsigned long)page_address(page),
               (unsigned long)page_address(page) + PAGE_SIZE);
}

/*
 * page is the kernel virtual addr and ARC_REG_DC_FLDL expects a phy addr
 */
void flush_dcache_page_virt(unsigned long *page)
{
    flush_dcache_range((unsigned long)page,
               (unsigned long)page + PAGE_SIZE);
}

void flush_dcache_all()
{
    unsigned long flags;

    local_irq_save(flags);
    write_new_aux_reg(ARC_REG_DC_FLSH, 1);

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & DC_FLUSH_STATUS_BIT) ;

    local_irq_restore(flags);
}

EXPORT_SYMBOL(flush_dcache_all);
/*
 * Fixme :: Might need to be corrected so that dma works correctly, leaving
 * for now.
 * All the addrs must be physical addrs, that means the addr
 * translation must be done before writing the addr to the cache line.
 */

inline void flush_and_inv_dcache_range(unsigned long start, unsigned long end)
{
    unsigned long flags, dc_ctrl;

    start &= DCACHE_LINE_MASK;
    local_irq_save(flags);

    /* Set the Invalidate mode to FLUSH BEFORE INV */
    dc_ctrl = read_new_aux_reg(ARC_REG_DC_CTRL);
	write_new_aux_reg(ARC_REG_DC_CTRL, dc_ctrl | INV_MODE_FLUSH);

    /* Invoke Cache INV CMD */
    while (end > start) {
        write_new_aux_reg(ARC_REG_DC_IVDL, start);
        start = start + ARC_DCACHE_LINE_LEN;
    }

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & DC_FLUSH_STATUS_BIT) ;

    /* Switch back the DISCARD ONLY Invalidate mode */
	write_new_aux_reg(ARC_REG_DC_CTRL, dc_ctrl & ~INV_MODE_FLUSH);

    local_irq_restore(flags);
}

inline void inv_dcache_range(unsigned long start, unsigned long end)
{
    unsigned long flags;

    start &= DCACHE_LINE_MASK;
    local_irq_save(flags);

    /* Default Invalidate mode is already DISCARD only
       Throw away the Dcache lines */
    while (end > start) {
        write_new_aux_reg(ARC_REG_DC_IVDL, start);
        start = start + ARC_DCACHE_LINE_LEN;
    }

    local_irq_restore(flags);
}

inline void inv_dcache_all()
{
    unsigned long flags;

    local_irq_save(flags);

    /* Throw away the contents of entrie Dcache */
    write_new_aux_reg(ARC_REG_DC_IVDC, 0x1);

    local_irq_restore(flags);
}

EXPORT_SYMBOL(flush_dcache_range);
EXPORT_SYMBOL(inv_dcache_range);
EXPORT_SYMBOL(flush_dcache_page);
#endif

#ifdef CONFIG_ARC700_USE_ICACHE

void flush_icache_all()
{
    unsigned long flags;

    local_irq_save(flags);

    write_new_aux_reg(ARC_REG_IC_IVIC, 1);

    /*
     * lr will not complete till the icache inv operation is not over
     */
    read_new_aux_reg(ARC_REG_IC_CTRL);
    local_irq_restore(flags);
}

/*
 * start, end - PHY Address
 */

void __flush_icache_range(unsigned long start, unsigned long end)
{
    unsigned long flags;

    start &= ICACHE_LINE_MASK;
    local_irq_save(flags);

    while (end > start) {
        write_new_aux_reg(ARC_REG_IC_IVIL, start);

        // Invalidates the cache lines in sets x, x+256..
        // to address the cache aliasing problem.

        if (unlikely(arc_cache_meta.has_aliasing & ARC_IC_ALIASING))
        {
            if( arc_cache_meta.i_sz == 32768)
                write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
            else if( arc_cache_meta.i_sz == 65536) {
                write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
                write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x10);
                write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x11);
            }
        }

        start = start + ARC_ICACHE_LINE_LEN;
    }

    local_irq_restore(flags);
}

/* This is called on insmod, with kernel virtual address for CODE of
 * the module. ARC cache maintenance ops require PHY address thus we
 * need to convert vmalloc addr to PHY addr
 */
void flush_icache_range(unsigned long kstart, unsigned long kend)
{
    int sz;
    unsigned long phy, pfn;
    unsigned long flags;

    //printk("Kernel Cache Cohenercy: %lx to %lx\n",kstart, kend);

    /* If this is called for user virtual address, flush the icache */
    if (kstart < TASK_SIZE || kstart > PAGE_OFFSET) {
        __flush_icache_range(kstart, kend);
        return;
    }

    //  For insmod case, make sure both I and C caches are coheremt
    sz = kend - kstart + 1;
    if (sz > PAGE_SIZE) {
        flush_cache_all();
        return;
    }

    while (sz > 0) {
        pfn = vmalloc_to_pfn((void *)kstart);
        phy = pfn << PAGE_SHIFT;
        //printk("Flushing for virt %lx Phy %lx PAGE %lx\n", kstart, phy, pfn);
        local_irq_save(flags);
        flush_dcache_range(phy, phy+PAGE_SIZE );
        __flush_icache_range(phy, phy+PAGE_SIZE);
        local_irq_restore(flags);
        kstart += PAGE_SIZE;
        sz -= PAGE_SIZE;
    }

}

/*
 * This is called when a page-cache page is about to be mapped into a
 * user process' address space.  It offers an opportunity for a
 * port to ensure d-cache/i-cache coherency if necessary.
 */

void flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
    if (!(vma->vm_flags & VM_EXEC))
        return;

    __flush_icache_range((unsigned long)page_address(page),
               (unsigned long)page_address(page) + PAGE_SIZE);
}

void flush_cache_all()
{
    unsigned long flags;

    local_irq_save(flags);

    flush_icache_all();

    flush_and_inv_dcache_all();

    local_irq_restore(flags);

}

void flush_cache_mm(struct mm_struct *mm)
{
    if (mm->context.asid != NO_ASID)
        flush_cache_all();
}

/*
 * FIXME: start and end user virtual addrs. How to ensure the correct cache
 * is getting invalidate??
 */

void flush_cache_range(struct vm_area_struct *vma, unsigned long start,
               unsigned long end)
{
    struct mm_struct *mm = vma->vm_mm;
    if (mm->context.asid != NO_ASID)
        flush_cache_all();
}

/* Sameer : I just added new second arg user_addr to match the signature
            expected by arch-independent code. We are yet not doing with
            the second arg which we need to do */

void flush_cache_page(struct vm_area_struct *vma, unsigned long page,
              unsigned long pfn)
{
    struct mm_struct *mm = vma->vm_mm;
    pgd_t *pgdp;
    pmd_t *pmdp;
    pte_t *ptep;
    unsigned long paddr;

    if (mm->context.asid == NO_ASID)
        return;

    page &= PAGE_MASK;
    pgdp = pgd_offset(mm, page);
    pmdp = pmd_offset(pgdp, page);
    ptep = pte_offset(pmdp, page);

    paddr = pte_val(*ptep);
    paddr &= ~(PAGE_SIZE - 1);
    /* If the page isn't marked present then it couldn't possibly
       be in the cache
     */
    if (!(pte_val(*ptep) & _PAGE_PRESENT))
        return;

    if ((mm == current->active_mm) && (pte_val(*ptep) & _PAGE_VALID)) {

        flush_dcache_page_virt((__va(paddr)));
        if (vma->vm_flags & VM_EXEC)
            flush_icache_page(vma, virt_to_page(__va(paddr)));
    }
}

#endif

void __init a7_cache_init(void)
{
    a7_probe_cache();
}
