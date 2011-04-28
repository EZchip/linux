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
 *  linux/include/asm-arc/div64.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Autors : Amit Bhor
 */

#ifndef _ASM_ARC_DIV64_H
#define _ASM_ARC_DIV64_H

#include <asm-generic/div64.h>

/* Sameer: This doesn't really work. */
/* #define do_div(n,base) ({					\ */
/* 	int __res;						\ */
/* 	__res = ((unsigned long) n) % (unsigned) base;		\ */
/* 	n = ((unsigned long) n) / (unsigned) base;		\ */
/* 	__res;							\ */
/* }) */

#endif	/*_ASM_ARC_DIV64_H*/
