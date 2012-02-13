/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * vineetg: Mar 2010
 *  -GCC can't generate ZOL for core cache flush loops.
 *   Conv them into iterations based as opposed to while (start < end) types
 *
 * Vineetg: July 2009
 *  -In I-cache flush routine we check for aliasing for every line INV.
 *   Instead now we setup routines per cache geometry and invoke them
 *   via function pointers.
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

extern struct cpuinfo_arc cpuinfo_arc700[];
static void __arc_icache_inv_lines_no_alias(unsigned long, int);
static void __arc_icache_inv_lines_32k(unsigned long, int);
static void __arc_icache_inv_lines_64k(unsigned long, int);

/* Holds the ptr to flush routine, dependign on size due to aliasing issues */
static void (* ___flush_icache_rtn)(unsigned long, int);

char * arc_cache_mumbojumbo(int cpu_id, char *buf)
{
    int num = 0;
    struct cpuinfo_arc_cache *p_cache = &cpuinfo_arc700[0].icache;

    num += sprintf(buf+num,"Detected I-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
            p_cache->assoc, p_cache->line_len, TO_KB(p_cache->sz));

#ifdef CONFIG_ARC700_USE_ICACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    p_cache = &cpuinfo_arc700[0].dcache;
    num += sprintf(buf+num,"Detected D-cache : \n");
    num += sprintf(buf+num,"  Type=%d way set-assoc, Line length=%u, Size=%uK",
            p_cache->assoc, p_cache->line_len, TO_KB(p_cache->sz));

#ifdef  CONFIG_ARC700_USE_DCACHE
    num += sprintf(buf+num," (enabled)\n");
#else
    num += sprintf(buf+num," (disabled)\n");
#endif

    return buf;
}

/* Read the Cache Build Confuration Registers, Decode them and save into
 * the cpuinfo structure for later use.
 * No Validation is done here, simply read/convert the BCRs
 */
void __init read_decode_cache_bcr(void)
{
    struct bcr_cache ibcr, dbcr;

#ifdef CONFIG_ARC700_USE_ICACHE
    {
        struct cpuinfo_arc_cache *p_ic = &cpuinfo_arc700[0].icache;
        READ_BCR(ARC_REG_I_CACHE_BUILD_REG, ibcr);

        if (ibcr.config == 0x3)
            p_ic->assoc = 2;

        p_ic->line_len = 8 << ibcr.line_len;
        p_ic->sz = 0x200 << ibcr.sz;
    }
#endif

#ifdef CONFIG_ARC700_USE_DCACHE
    {
        struct cpuinfo_arc_cache *p_dc = &cpuinfo_arc700[0].dcache;
        READ_BCR(ARC_REG_D_CACHE_BUILD_REG, dbcr);

        if (dbcr.config == 0x2)
            p_dc->assoc = 4; // 4 way set assoc

        p_dc->line_len = 16 << dbcr.line_len;
        p_dc->sz = 0x200 << dbcr.sz;
    }
#endif

}

/* 1. Validate the Cache Geomtery (compile time config matches hardware)
 * 2. If I-cache suffers from aliasing, setup work arounds (difft flush rtn)
 * 3. Enable the Caches, setup default flush mode for D-Cache
 * 3. Calculate the SHMLBA used by user space
 */
void __init arc_cache_init(void)
{
    struct cpuinfo_arc_cache *dc, *ic;
    unsigned int temp;

    ARC_shmlba = max(ARC_shmlba, (unsigned int)PAGE_SIZE);

    /***********************************************
     * I-Cache related init
     **********************************************/

#ifdef CONFIG_ARC700_USE_ICACHE
    ic = &cpuinfo_arc700[0].icache;

    /* 1. Confirm some of I-cache params which Linux assumes */
    if ( ( ic->assoc != ICACHE_COMPILE_WAY_NUM) ||
         ( ic->line_len != ICACHE_COMPILE_LINE_LEN ) ) {
        goto sw_hw_mismatch;
    }

    switch(ic->sz) {
    case 8 * 1024:
    case 16 * 1024:
        ___flush_icache_rtn = __arc_icache_inv_lines_no_alias;
        break;
    case 32 * 1024:
        ___flush_icache_rtn = __arc_icache_inv_lines_32k;
        break;
    case 64 * 1024:
        ___flush_icache_rtn = __arc_icache_inv_lines_64k;
        break;
    default:
        panic("Unsupported I-Cache Sz\n");
    }

    /* check whether Icache way size greater than PAGE_SIZE as it
     * cause Aliasing Issues and requires special handling
     */
    if ( (ic->sz / ICACHE_COMPILE_WAY_NUM) > PAGE_SIZE) {
        ic->has_aliasing = 1;
        ARC_shmlba = max(ARC_shmlba, ic->sz / ICACHE_COMPILE_WAY_NUM);
    }
#endif

    /* Enable/disable I-Cache */
    temp = read_new_aux_reg(ARC_REG_IC_CTRL);

#ifdef CONFIG_ARC700_USE_ICACHE
    temp &= (~BIT_IC_CTRL_CACHE_DISABLE);
#else
    temp |= (BIT_IC_CTRL_CACHE_DISABLE);
#endif

     write_new_aux_reg(ARC_REG_IC_CTRL, temp);

    /***********************************************
     * D-Cache related init
     **********************************************/

#ifdef CONFIG_ARC700_USE_DCACHE
    dc = &cpuinfo_arc700[0].dcache;

    if ( ( dc->assoc != DCACHE_COMPILE_WAY_NUM) ||
         ( dc->line_len != DCACHE_COMPILE_LINE_LEN ) ) {
        goto sw_hw_mismatch;
    }

    /* check for D-Cache aliasing */
    if ((dc->sz / DCACHE_COMPILE_WAY_NUM) > PAGE_SIZE) {
        dc->has_aliasing = 1;
        ARC_shmlba = max(ARC_shmlba, dc->sz / DCACHE_COMPILE_WAY_NUM);
    }
#endif

    /* Set the default Invalidate Mode to "simpy discard dirty lines"
     *  as this is more frequent then flush before invalidate
     * Ofcourse we toggle this default behviour when desired
     */
    temp = read_new_aux_reg(ARC_REG_DC_CTRL);
    temp &= ~BIT_DC_CTRL_INV_MODE_FLUSH;

#ifdef  CONFIG_ARC700_USE_DCACHE
    /* Enable D-Cache: Clear Bit 0 */
    write_new_aux_reg(ARC_REG_DC_CTRL, temp & (~BIT_IC_CTRL_CACHE_DISABLE));
#else
    /* Flush D cache */
    write_new_aux_reg(ARC_REG_DC_FLSH, 0x1);
    /* Disable D cache */
    write_new_aux_reg(ARC_REG_DC_CTRL, temp | BIT_IC_CTRL_CACHE_DISABLE);
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

/***********************************************************
 * Machine specific helpers for Entire D-Cache or Per Line ops
 **********************************************************/

/* Operation on Entire D-Cache
 * @cacheop = { 0x0 = DISCARD, 0x1 = WBACK, 0x3 = WBACK-N-INV }
 * @aux_reg: Relevant Register in Cache
 */
static inline void __arc_dcache_entire_op(int aux_reg, int cacheop)
{
    unsigned long flags, orig_d_ctrl = orig_d_ctrl;

    local_irq_save(flags);

    if (cacheop & 2) {
        /* Dcache provides 2 cmd: FLUSH or INV
         * INV inturn has sub-modes: DISCARD or FLUSH-BEFORE
         * Default INV sub-mode is DISCARD, hence this check
         */
        orig_d_ctrl = read_new_aux_reg(ARC_REG_DC_CTRL);
	    write_new_aux_reg(ARC_REG_DC_CTRL, orig_d_ctrl | BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    write_new_aux_reg(aux_reg, 0x1);

    if (cacheop & 1) {
        /* wait for the flush to complete, poll on the FS Bit */
        while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS) ;
    }

    if (cacheop & 2) {
        /* Switch back the DISCARD ONLY Invalidate mode */
	    write_new_aux_reg(ARC_REG_DC_CTRL,
						orig_d_ctrl & ~BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    local_irq_restore(flags);
}

/* Per Line Operation on D-Cache
 * Doesn't deal with type-of-op/IRQ-disabling/waiting-for-flush-to-complete
 * It's sole purpose is to help gcc generate ZOL
 */

static inline void __arc_dcache_per_line_op(unsigned long start, unsigned long sz, int aux_reg)
{
    int num_lines = sz/DCACHE_COMPILE_LINE_LEN;

    /* If @start address is not cache line aligned, do so.
     * This also means 1 extra line to be flushed
     *
     * However instead of doing this unconditionally, we can optimise a bit.
     * Page sized flush ops can be assumed to begin on a cache line (if not
     * page aligned). Thus alignment check can be avoided when @sz = PAGE_SIZE
     * Since page wise flush ops pass a constant @sz = PAGE_SIZE, and this
     * function is inline, gcc can do "constant propagation" and compile away
     * the entire alignment code block.
     *      -outer "if" will be eliminated by constant propagation
     *     -Since outer "if" will compile time evaluate to false,
     *      inner block will be thrown away as well
     *
     * However when @sz is not constant, the exta size check is superflous
     * as we want to unconditionally do the aligment check, hence the
     * __builtin_constant_p.
     */
    if ( ! __builtin_constant_p(sz) || sz != PAGE_SIZE ) {
        if (start & ~DCACHE_LINE_MASK) {
            start &= DCACHE_LINE_MASK;
            num_lines++;
        }
    }

    while (num_lines-- > 0) {
        write_new_aux_reg(aux_reg, start);
        start += DCACHE_COMPILE_LINE_LEN;
    }
}

/* D-Cache : Per Line FLUSH (wback)
 */

static inline void __arc_dcache_flush_lines(unsigned long start, unsigned long sz)
{
    unsigned long flags;

    local_irq_save(flags);

    __arc_dcache_per_line_op(start, sz, ARC_REG_DC_FLDL);

    /* wait for the flush to complete, poll on the FS Bit */
    while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS) ;

    local_irq_restore(flags);
}

/* D-Cache : Per Line INV (discard or wback+discard)
 */

static inline void __arc_dcache_inv_lines(unsigned long start, unsigned long sz,
                                    int flush_n_inv)
{
    unsigned long flags, orig_d_ctrl = orig_d_ctrl;

    local_irq_save(flags);

    if (flush_n_inv) {
        /* Dcache provides 2 cmd: FLUSH or INV
         * INV inturn has sub-modes: DISCARD or FLUSH-BEFORE
         * Default INV sub-mode is DISCARD, hence this check
         */
        orig_d_ctrl = read_new_aux_reg(ARC_REG_DC_CTRL);
	    write_new_aux_reg(ARC_REG_DC_CTRL, orig_d_ctrl | BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    __arc_dcache_per_line_op(start, sz, ARC_REG_DC_IVDL);


    if (flush_n_inv) {
        /* wait for the flush to complete, poll on the FS Bit */
        while (read_new_aux_reg(ARC_REG_DC_CTRL) & BIT_DC_CTRL_FLUSH_STATUS) ;

        /* Switch back the DISCARD ONLY Invalidate mode */
	    write_new_aux_reg(ARC_REG_DC_CTRL, orig_d_ctrl & ~BIT_DC_CTRL_INV_MODE_FLUSH);
    }

    local_irq_restore(flags);
}

/***********************************************************
 * Exported APIs
 **********************************************************/

void inv_dcache_all()
{
    /* Throw away the contents of entrie Dcache */
    __arc_dcache_entire_op(ARC_REG_DC_IVDC, 0x0);
}

void flush_and_inv_dcache_all(void)
{
    __arc_dcache_entire_op(ARC_REG_DC_IVDC, 0x3);
}

void flush_dcache_all()
{
    __arc_dcache_entire_op(ARC_REG_DC_FLSH, 0x1);
}


void flush_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_flush_lines(start, end - start);
}

void flush_dcache_page(struct page *page)
{
    __arc_dcache_flush_lines((unsigned long)page_address(page), PAGE_SIZE);
}

/*
 * page is the kernel virtual addr and ARC_REG_DC_FLDL expects a phy addr
 * TODO: need to remove this
 */
void flush_dcache_page_virt(unsigned long *page)
{
    __arc_dcache_flush_lines((unsigned long)page,PAGE_SIZE);
}


void flush_and_inv_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_inv_lines(start, end-start, 1);
}

void inv_dcache_range(unsigned long start, unsigned long end)
{
    __arc_dcache_inv_lines(start, end-start, 0);
}

EXPORT_SYMBOL(flush_dcache_range);
EXPORT_SYMBOL(inv_dcache_range);
EXPORT_SYMBOL(flush_dcache_all);
EXPORT_SYMBOL(flush_and_inv_dcache_all);
EXPORT_SYMBOL(flush_dcache_page);

#endif

#ifdef CONFIG_ARC700_USE_ICACHE

/***********************************************************
 * Machine specific helpers for per line I-Cache invalidate
 * We have 3 different routines to take care of I-cache aliasing
 * in specific configurations of ARC700
 **********************************************************/

/*
 * start, end - PHY Address
 */

static void __arc_icache_inv_lines_no_alias(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines_32k(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines_64k(unsigned long start, int num_lines)
{
    while (num_lines-- > 0) {
        write_new_aux_reg(ARC_REG_IC_IVIL, start);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x01);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x10);
        write_new_aux_reg(ARC_REG_IC_IVIL, start | 0x11);
        start += ICACHE_COMPILE_LINE_LEN;
    }
}

static void __arc_icache_inv_lines(unsigned long start, unsigned long sz)
{
    unsigned long flags;
    int num_lines = sz / ICACHE_COMPILE_LINE_LEN;

    if ( ! __builtin_constant_p(sz) || sz != PAGE_SIZE ) {
        if (start & ~ICACHE_LINE_MASK) {
            start &= ICACHE_LINE_MASK;
            num_lines++;
        }
    }

    local_irq_save(flags);
    (*___flush_icache_rtn)(start, num_lines);
    local_irq_restore(flags);
}

/***********************************************************
 * Exported APIs
 **********************************************************/

/* This is API for making I/D Caches consistent when modifying code
 * (loadable modules, kprobes,  etc)
 * This is called on insmod, with kernel virtual address for CODE of
 * the module. ARC cache maintenance ops require PHY address thus we
 * need to convert vmalloc addr to PHY addr
 */
void flush_icache_range(unsigned long kstart, unsigned long kend)
{
    int tot_sz;
    unsigned long phy, pfn;
    unsigned long flags;

    //printk("Kernel Cache Cohenercy: %lx to %lx\n",kstart, kend);

    /* This is not the right API for user virtual address */
    if (kstart < TASK_SIZE) {
        BUG_ON("Flush icache range for user virtual addr space");
        return;
    }

    /* Shortcut for bigger flush ranges.
     * Here we dont care if this was kernel virtual or phy addr
     */
    tot_sz = kend - kstart + 1;
    if (tot_sz > PAGE_SIZE) {
        flush_cache_all();
        return;
    }

    /* Now it could be kernel virtual addr space (0x7000_0000 to 0x7FFF_FFFF)
     * or Kernel Physical addr space (0x8000_0000 onwards)
     * The caveat is ARC700 Cache flushes require PHY address, thus need
     * for seperate handling for virtual addr
     */

    /* Kernel Paddr */
    if (kstart > PAGE_OFFSET) {
        __arc_icache_inv_lines(kstart, kend-kstart);
        __arc_dcache_flush_lines(kstart, kend-kstart);
        return;
    }

    /* Kernel Vaddr */
    while (tot_sz > 0) {
        int sz;
        pfn = vmalloc_to_pfn((void *)kstart);
        phy = pfn << PAGE_SHIFT;
        sz = (tot_sz >= PAGE_SIZE)? PAGE_SIZE : tot_sz;
        //printk("Flushing for virt %lx Phy %lx PAGE %lx\n", kstart, phy, pfn);
        local_irq_save(flags);
        __arc_dcache_flush_lines(phy, sz);
        __arc_icache_inv_lines(phy, sz);
        local_irq_restore(flags);
        kstart += PAGE_SIZE;
        tot_sz -= PAGE_SIZE;
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

    __arc_icache_inv_lines((unsigned long)page_address(page), PAGE_SIZE);
}

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

void flush_cache_page(struct vm_area_struct *vma, unsigned long page,
              unsigned long pfn)
{
    unsigned int start = pfn << PAGE_SHIFT;

    __arc_dcache_flush_lines(start, PAGE_SIZE);

    if (vma->vm_flags & VM_EXEC)
        __arc_icache_inv_lines(start, PAGE_SIZE);

}

#endif
