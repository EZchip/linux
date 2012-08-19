/*******************************************************************************

  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#ifndef __PLAT_MEMMAP_H
#define __PLAT_MEMMAP_H

#define UART0_BASE				0xC0000000
#define	CPU_REGS_BASE			0xC0002000

#define REG_CPU_HW_VER     		(unsigned int *)(CPU_REGS_BASE + 0x3FC)
#define HW_VER_MAJOR_SHIFT		8
#define HW_VER_MINOR_SHIFT		0
#define HW_VER_MAJOR_MASK		0xFF00
#define HW_VER_MINOR_MASK		0x00FF

#define DCCM_COMPILE_BASE		0xA0000000
#define DCCM_COMPILE_SZ			(64 * 1024)
#define ICCM_COMPILE_SZ			(64 * 1024)

/* FMT & CCM area
 * It is 129MB @ 0x57f00000:0x5fffffff.
 * Marks how much space to reserve before STACK_TOP
 */
#define PLAT_RESERVED_GUTTER	0x08100000

#endif
