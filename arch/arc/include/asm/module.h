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
 *  linux/include/asm-arc/module.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Amit Bhor, Sameer Dhavale
 */

#ifndef _ASM_ARC_MODULE_H
#define _ASM_ARC_MODULE_H

struct mod_arch_specific
{
};

#define Elf_Shdr	Elf32_Shdr
#define Elf_Sym		Elf32_Sym
#define Elf_Ehdr	Elf32_Ehdr

#define MODULE_PROC_FAMILY "ARC700"

#define MODULE_ARCH_VERMAGIC MODULE_PROC_FAMILY
#endif /* _ASM_ARC_MODULE_H */
