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
 *  linux/include/asm-arc/string.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor
 */

#ifndef _ASM_ARC_STRING_H
#define _ASM_ARC_STRING_H

#define __HAVE_ARCH_MEMSET

extern void * memset(void *, int, __kernel_size_t);

#define memzero(buff, len)	memset(buff, 0, len)

#define __HAVE_ARCH_MEMCPY

extern void * memcpy(void *, const void *, __kernel_size_t);



#endif
