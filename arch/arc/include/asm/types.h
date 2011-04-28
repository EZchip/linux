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
 *  linux/include/asm-arc/types.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor
 */

#ifndef _ASM_ARC_TYPES_H
#define _ASM_ARC_TYPES_H

#include <asm-generic/int-ll64.h>

/* Sameer: Shuffling and juggling with __ASSEMBLY__ */
#ifndef __ASSEMBLY__

typedef unsigned short umode_t;

#endif /* !__ASSEMBLY__ */
/*
 * These aren't exported outside the kernel to avoid name space clashes
 */
#ifdef __KERNEL__

#define BITS_PER_LONG 32

#ifndef __ASSEMBLY__

/* Dma addresses are 32-bits wide.  */
typedef u32 dma_addr_t;

#endif /* !__ASSEMBLY__ */


#endif /* __KERNEL__ */


#endif
