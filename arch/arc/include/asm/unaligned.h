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
 *  linux/include/asm-arcnommu/unaligned.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor
 * Borrowed from the sh code
 */



#ifndef _ASM_ARC_UNALIGNED_H
#define _ASM_ARC_UNALIGNED_H

/* ARC700 can't handle unaligned accesses. */

#include <linux/string.h>
#include <linux/unaligned/le_struct.h>
#include <linux/unaligned/be_byteshift.h>

/* Use memmove here, so gcc does not insert a __builtin_memcpy. */

#define get_unaligned(ptr) \
  ({ __typeof__(*(ptr)) __tmp; memmove(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#define put_unaligned(val, ptr)				\
  ({ __typeof__(*(ptr)) __tmp = (val);			\
     memmove((ptr), &__tmp, sizeof(*(ptr)));		\
     (void)0; })

#endif /* _ASM_ARC_UNALIGNED_H */
