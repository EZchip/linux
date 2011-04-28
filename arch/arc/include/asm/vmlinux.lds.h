#ifndef __ASM_VMLINUX_LDS_H__
#define __ASM_VMLINUX_LDS_H__

#ifdef CONFIG_ARCH_ARC_DCCM
#define ARCFP_CCM_DATA      *(.data.arcfp)
#define ARCFP_SDRAM_DATA
#else
#define ARCFP_CCM_DATA
#define ARCFP_SDRAM_DATA    *(.data.arcfp)
#endif

#ifdef CONFIG_ARCH_ARC_ICCM
#define ARCFP_CCM_TEXT      *(.text.arcfp)
#define ARCFP_SDRAM_TEXT
#else
#define ARCFP_CCM_TEXT
#define ARCFP_SDRAM_TEXT    *(.text.arcfp)
#endif

#endif
