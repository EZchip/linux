/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_ASM_CACHE_H
#define __ARC_ASM_CACHE_H

/* In case $$ not config, setup a dummy number for rest of kernel */
#ifndef CONFIG_ARC_CACHE_LINE_SHIFT
#define L1_CACHE_SHIFT      5
#else
#define L1_CACHE_SHIFT      CONFIG_ARC_CACHE_LINE_SHIFT
#endif

#define L1_CACHE_BYTES      ( 1 << L1_CACHE_SHIFT )
#define L1_CACHE_ALIGN(x)   ((((unsigned int)(x))+(L1_CACHE_BYTES-1)) & \
                                ~(L1_CACHE_BYTES-1))

#define ICACHE_COMPILE_WAY_NUM      2
#define DCACHE_COMPILE_WAY_NUM      4

/* Helpers */
#define ICACHE_COMPILE_LINE_LEN     L1_CACHE_BYTES
#define DCACHE_COMPILE_LINE_LEN     L1_CACHE_BYTES

#define ICACHE_LINE_MASK     (~(ICACHE_COMPILE_LINE_LEN - 1))
#define DCACHE_LINE_MASK     (~(DCACHE_COMPILE_LINE_LEN - 1))

#if ICACHE_COMPILE_LINE_LEN != DCACHE_COMPILE_LINE_LEN
#error "Need to fix some code as I/D cache lines not same"
#else
#define is_not_cache_aligned(p) ((unsigned long)p & (~DCACHE_LINE_MASK))
#endif


#ifndef __ASSEMBLY__

/* Uncached access macros */
#define arc_read_uncached_32(ptr)                   \
({                                  \
    unsigned int __ret;                     \
    __asm__ __volatile__ ("ld.di %0, [%1]":"=r"(__ret):"r"(ptr));   \
    __ret;                              \
 })

#define arc_write_uncached_32(ptr, data)                \
({                                  \
    __asm__ __volatile__ ("st.di %0, [%1]"::"r"(data), "r"(ptr));   \
 })

/* used to give SHMLBA a value to avoid Cache Aliasing */
extern unsigned int ARC_shmlba ;


#endif

#endif /* _ASM_CACHE_H */
