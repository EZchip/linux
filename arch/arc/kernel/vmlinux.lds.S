/* ld script to make ARC Linux kernel
 * taken from the armnommu/ version by Martin Mares
 *
 * Copyright ARC International
 *
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/page.h>

OUTPUT_ARCH(arc)
ENTRY(_stext)

jiffies = jiffies_64;


SECTIONS
{
    . = 0x80000000;
    _int_vec_base_lds = .;
    .vector : {
        *(.vector)
    }

    . = ALIGN(PAGE_SIZE);
    _stext = .;
    .init : {           /* Init code and data       */
        _init_begin = .;

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
#ifdef CONFIG_BLK_DEV_INITRD
        . = ALIGN(PAGE_SIZE);
        __initramfs_start = .;
            *(.init.ramfs)
        __initramfs_end = .;
#endif
        . = ALIGN(PAGE_SIZE);
        __per_cpu_start = .;
            *(.data.percpu)
        __per_cpu_end = .;
    }

	.init.rodata : {
		*(.init.rodata)
	}

    . = ALIGN(PAGE_SIZE);
    _init_end = .;

    .text : {           /* Real text segment        */
        _text = .;      /* Text and read-only data  */
            TEXT_TEXT
            SCHED_TEXT
            LOCK_TEXT
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
        *(.gnu.linkonce.arcextmap.*)
    }

    STABS_DEBUG

    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_macinfo 0 : { *(.debug_macinfo) }
}
