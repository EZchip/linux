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
 *  linux/include/asm-arc/byteorder.h
 *
 *
 */
#ifndef __ASM_ARC_BYTEORDER_H
#define __ASM_ARC_BYTEORDER_H


#include <asm/types.h>

#if !defined(__STRICT_ANSI__) || defined(__KERNEL__)
#  define __BYTEORDER_HAS_U64__
#  define __SWAB_64_THRU_32__
#endif

#ifndef	__big_endian__
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
#define LITTLE_ENDIAN
#include <linux/byteorder/little_endian.h>
#else
#define __BIG_ENDIAN__
#define BIG_ENDIAN
#include <linux/byteorder/big_endian.h>
#endif	/* __big_endian__ */


#endif	/* ASM_ARC_BYTEORDER_H */
