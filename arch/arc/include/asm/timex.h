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
 *  linux/include/asm-arc/timex.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor
 */

#ifndef _ASM_ARC_TIMEX_H
#define _ASM_ARC_TIMEX_H

/* Sameer: Required to calculate tick_length in ntp.c */
/* Sameer: Won't work if clock speed is a boot-time parameter. */
#define CLOCK_TICK_RATE	CONFIG_ARC700_CLK /* Underlying HZ */

typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	return 0;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif /* _ASM_ARC_TIMEX_H */
