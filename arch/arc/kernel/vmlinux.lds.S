/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/vmlinux.lds.h>
#include <asm/cache.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/board/plat_memmap.h>

OUTPUT_ARCH(arc)
ENTRY(_stext)

#ifdef CONFIG_CPU_BIG_ENDIAN
	jiffies  = jiffies_64 + 4;
#else
	jiffies  = jiffies_64;
#endif


SECTIONS
{
    /* ICCM starts at 0x8000_0000. So if kernel is relocated to some other
     * address, make sure peripheral at 0x8z doesn't clash with ICCM
     * Essentially vector is also in ICCM.
     */

    . = CONFIG_LINUX_LINK_BASE;

    _int_vec_base_lds = .;
    .vector : {
        *(.vector)
        . = ALIGN(PAGE_SIZE);
    }

    /* TODO: These might change depending on H/w */
#ifdef CONFIG_ARCH_ARC_ICCM
    .text.arcfp : {
        ARCFP_CCM_TEXT
        . = ALIGN(ICCM_COMPILE_SZ);
    }
#endif


/************************************************************************
 * vineetg: Feb 2010: Responsibe for the weird arrangement of .init stuff
 *
 *   Instead of a more intuitive arrangement as follows
 *
 *   (1a) @marker _stext
 *       .init {
 *   (2a)    @marker _init_begin
 *   (3a)    @marker _sinitext
 *          *(init.text)
 *          ...
 *   (4a)    *(.init.ramfs) => gobbled up
 *          ...
 *       }
 *
 *   we have
 *
 *   (2b) @marker _init_begin  => outside of .init { }
 *   (4b) .init.ramfs {   => seperate subsection of .init
 *       *(.init.ramfs)
 *       }
 *   (1b) @marker _stext
 *       .init {
 *   (3b)    @marker _sinitext
 *          *(init.text)
 *
 *   The reason for having a seperate subsection .init.initramfs is to
 *   prevent objump from including it in kernel dumps
 *
 *   Reason for having .init.ramfs above .init is to make sure that the
 *   binary blob is tucked away to one side, reducing the displacement
 *   between .init.text and .text, avoiding any possible relocation errors
 *   because of calls from .init.text to .text
 *   Yes such calls do exit: decompress_inflate.c:gunzip( ) -> calls
 *   zlib_inflate_workspace( )
 *
 ************************************************************************/

        _init_begin = .;

    .init.ramfs : {
        INIT_RAM_FS
    }

    . = ALIGN(PAGE_SIZE);
    _stext = .;
    .init : {           /* Init code and data       */
        _sinittext = .;
            INIT_TEXT
        _einittext = .;

        INIT_DATA

        INIT_SETUP(16)
        INIT_CALLS
        CON_INITCALL
        SECURITY_INITCALL

        __tagtable_begin = .;
            *(.taglist.init)
        __tagtable_end = .;

    }

	PERCPU_SECTION(L1_CACHE_BYTES)
    . = ALIGN(PAGE_SIZE);
    _init_end = .;

    .text : {           /* Real text segment        */
        _text = .;      /* Text and read-only data  */
            TEXT_TEXT
            ARCFP_SDRAM_TEXT
            SCHED_TEXT
            LOCK_TEXT
            KPROBES_TEXT
            *(.fixup)
            *(.gnu.warning)

    }
    EXCEPTION_TABLE(16)
    _etext = .;     /* End of text section      */

    /* writeable */
    _sdata = .;

    RO_DATA_SECTION(PAGE_SIZE)

    /* 1. this is .data essentially
       2. THREAD_SIZE for init.task, must be kernel-stk sz aligned
     */
    RW_DATA_SECTION(L1_CACHE_BYTES, PAGE_SIZE, THREAD_SIZE)

    /* becasuse above RW_xxx doesn't include our .data.arcfp */
#ifndef CONFIG_ARCH_ARC_DCCM
    .data.arcfp : {
    *(.data..shared_aligned)
    ARCFP_SDRAM_DATA
    }
#endif

    _edata = .;

    BSS_SECTION(0, 0, 0)

    . = ALIGN(PAGE_SIZE);
    __start_unwind = .;
    .debug_frame  : { *(.debug_frame) }
    __end_unwind = .;

    . = ALIGN(PAGE_SIZE);
    end_kernel = .;

    _end = . ;

    STABS_DEBUG
    DISCARDS

    .arcextmap 0 : {
        *(.gnu.linkonce.arcextmap.*)
        *(.arcextmap.*)
     }

    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_macinfo 0 : { *(.debug_macinfo) }

#ifdef CONFIG_ARCH_ARC_DCCM
    . = DCCM_COMPILE_BASE;
    __arc_dccm_base = .;
    .data.arcfp : {
        *(.data..shared_aligned)
        ARCFP_CCM_DATA
    }
    . = ALIGN(DCCM_COMPILE_SZ);
#endif
}
