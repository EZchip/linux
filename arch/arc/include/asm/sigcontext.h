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
 *  linux/include/asm-arc/sigcontext.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor
 */

#ifndef _ASM_ARC_SIGCONTEXT_H
#define _ASM_ARC_SIGCONTEXT_H

/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.  Note: only add new entries
 * to the end of the structure.
 */
struct sigcontext {
        unsigned long   oldmask;
        unsigned long   sp;
	unsigned long	bta;
        unsigned long	lp_start;
	unsigned long	lp_end;
	unsigned long	lp_count;
	unsigned long	status32;
	unsigned long	ret;	/* ilink1 or inlink2 */
	unsigned long	blink;
	unsigned long	fp;
	unsigned long	r26;	/* gp */
	unsigned long	r12;
	unsigned long	r11;
	unsigned long	r10;
	unsigned long	r9;
	unsigned long	r8;
	unsigned long	r7;
	unsigned long	r6;
	unsigned long	r5;
	unsigned long	r4;
	unsigned long	r3;
	unsigned long	r2;
	unsigned long	r1;
	unsigned long	r0;
};


#endif	/* _ASM_ARC_SIGCONTEXT_H */
