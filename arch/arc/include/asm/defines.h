/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_ASM_DEFINES_H__
#define __ARC_ASM_DEFINES_H__

#if defined(CONFIG_ARC_MMU_V1)
#define CONFIG_ARC_MMU_VER 1
#elif defined(CONFIG_ARC_MMU_V2)
#define CONFIG_ARC_MMU_VER 2
#elif defined(CONFIG_ARC_MMU_V3)
#define CONFIG_ARC_MMU_VER 3
#endif

/* Support for Load-locked/Store Conditional
 *  for doing fast Read-Modify-Write on uni-processor systems
 */
#if defined(CONFIG_ARC700_V_4_10) && defined(__Xlock)
#define ARC_HAS_LLSC
#else
#undef ARC_HAS_LLSC
#endif

/* Support for single cycle Endian Swap insn */
#if defined(CONFIG_ARC700_V_4_10) && defined(__Xswape)
#define ARC_HAS_SWAPE
#else
#undef ARC_HAS_SWAPE
#endif

/* Lot of hand written asm routines which get inlined as well contain ZOL
 *   copy_from_user(), clear_user(), memset_aligned( ) etc
 * Enabling these toggles prevents their inlining, confining ZOL to very few
 * routines - good test to check if arc-gcc is generating ZOLs or not
 */
#undef  NONINLINE_USR_CPY
#undef  NONINLINE_MEMSET

#endif  /* __ARC_ASM_DEFINES_H__ */
