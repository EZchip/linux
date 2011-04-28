/* ld script to make ARC Linux kernel
 * taken from the armnommu/ version by Martin Mares
 *
 * Copyright ARC International
 *
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/vmlinux.lds.h>
#include <asm/page.h>
#include <asm/board/plat_memmap.h>

OUTPUT_ARCH(arc)
ENTRY(_stext)

jiffies = jiffies_64;


SECTIONS
{
    /* ICCM starts at 0x8000_0000. So if kernel is relocated to some other
     * address, make sure peripheral at 0x8z doesn't clash with ICCM
     * Essentially vector is also in ICCM.
     */

    . = 0x80000000;
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

#ifdef CONFIG_BLK_DEV_INITRD
    .init.ramfs : {
        . = ALIGN(PAGE_SIZE);
        __initramfs_start = .;
            *(.init.ramfs)
        __initramfs_end = .;
    }
#endif

    . = ALIGN(PAGE_SIZE);
    _stext = .;
    .init : {           /* Init code and data       */
        _sinittext = .;
            INIT_TEXT
        _einittext = .;

            INIT_DATA
        . = ALIGN(16);

        __setup_start = .;
            *(.init.setup)
        __setup_end = .;

        __initcall_start = .;
            INITCALLS
        __initcall_end = .;

        __con_initcall_start = .;
            *(.con_initcall.init)
        __con_initcall_end = .;

        __tagtable_begin = .;
            *(.taglist.init)
        __tagtable_end = .;

        . = ALIGN(PAGE_SIZE);
        __per_cpu_start = .;
            *(.data.percpu)
        __per_cpu_end = .;

		*(.init.rodata)

    . = ALIGN(PAGE_SIZE);
    _init_end = .;

   }

    .text : {           /* Real text segment        */
        _text = .;      /* Text and read-only data  */
            TEXT_TEXT
            ARCFP_SDRAM_TEXT
            SCHED_TEXT
            LOCK_TEXT
            KPROBES_TEXT
            *(.fixup)
            *(.gnu.warning)


        . = ALIGN(16);
        __start___ex_table = .; /* Exception table      */
            *(__ex_table)
        __stop___ex_table = .;
    }
    _etext = .;     /* End of text section      */

    RO_DATA(PAGE_SIZE)

    /* writeable */
    . = ALIGN(PAGE_SIZE);
    .data : {

        *(.init.task)
        . = ALIGN(32);
        *(.data.cacheline_aligned)
        DATA_DATA
        ARCFP_SDRAM_DATA

        CONSTRUCTORS
        _edata = .;
    }

    .bss : {
        __bss_start = .;    /* BSS              */
        *(.bss)
        __bss_end = .;
        *(COMMON)
        _end = . ;
    }


    . = ALIGN(PAGE_SIZE);
    __start_unwind = .;
    .debug_frame  : { *(.debug_frame) }
    __end_unwind = .;

    . = ALIGN(PAGE_SIZE);
    end_kernel = .;

    /* Sections to be discarded */
    /DISCARD/ : {           /* Exit code and data       */
        EXIT_TEXT
        EXIT_DATA
        *(.exitcall.exit)
    }

    STABS_DEBUG

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
        ARCFP_CCM_DATA
    }
    . = ALIGN(DCCM_COMPILE_SZ);
#endif
}
