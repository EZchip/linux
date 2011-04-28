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
 * /include/asm-arc/a.out.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Rahul Trivedi
 */

#ifndef _ASM_ARC_A_OUT_H
#define _ASM_ARC_A_OUT_H

struct exec
{
	unsigned long a_info;         /* Use macros N_MAGIC, etc for access */
	unsigned a_text;              /* length of text, in bytes */
	unsigned a_data;              /* length of data, in bytes */
	unsigned a_bss;               /* length of uninitialized data area for file, in bytes */
	unsigned a_syms;              /* length of symbol table data in file, in bytes */
	unsigned a_entry;             /* start address */
	unsigned a_trsize;            /* length of relocation info for text, in bytes */
	unsigned a_drsize;            /* length of relocation info for data, in bytes */
};


#define N_TRSIZE(a)     ((a).a_trsize)
#define N_DRSIZE(a)     ((a).a_drsize)
#define N_SYMSIZE(a)    ((a).a_syms)

#endif /*  _ASM_ARC_A_OUT_H */
