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

#ifndef __PLAT_EZNPS_SMP_H
#define __PLAT_EZNPS_SMP_H

#ifdef CONFIG_SMP
#include <linux/types.h>
#include <asm/arcregs.h>

union RTC_64 {
	unsigned long long retValue;
	unsigned long clk_value[2];
};

#ifndef CONFIG_NR_NPS_HW_THREADS
#define CONFIG_NR_NPS_HW_THREADS	16
#endif

#define NR_CORES						(NR_CPUS/CONFIG_NR_NPS_HW_THREADS)
#define NPS_NUM_OF_CLUSTER_PER_COL_ROW	4
#define NPS_MSU_BLOCK_ID				0x18
#define NPS_CPU_MEM_ADDRESS_MASK		0x3FF
#define NPS_DEFAULT_MSID_SHIFT			4
#define NPS_DEFAULT_MSID				0x1e

#define NPS_CLUSTER_TO_X_AXIS(cl)		(cl / NPS_NUM_OF_CLUSTER_PER_COL_ROW)
#define NPS_CLUSTER_TO_Y_AXIS(cl)		(cl % NPS_NUM_OF_CLUSTER_PER_COL_ROW)
#define NPS_CPU_TO_CLUSTER(cpu)			((cpu >> 8) & 0xF)
#define NPS_CPU_TO_CORE_NUM(cpu)		((cpu >> 4) & 0xF)
#define NPS_CPU_TO_THREAD_NUM(cpu)		((cpu >> 0) & 0xF)
#define NPS_CPU_MTM_ID(th)				(((th >> 2) << 4) + (th % 4))

#define NPS_IO_MEM_ADDRESS(x, y, block_id, offset) \
	(unsigned int *)((0xF7000000 | \
				(x << 19) | \
				(y << 15) | \
				(block_id << 9) | \
				(offset << 2)))

#define NPS_IO_MEM_REG_THREAD(cl, id, offset) \
	NPS_IO_MEM_ADDRESS(NPS_CLUSTER_TO_X_AXIS(cl),\
				NPS_CLUSTER_TO_Y_AXIS(cl),\
				id,\
				offset)

#define NPS_IO_MEM_REG_CPU(cpu, id, offset)	\
	NPS_IO_MEM_REG_THREAD(\
				NPS_CPU_TO_CLUSTER(cpu), \
				id, \
				offset)

#define NPS_CPU_MSU_REGS(cpu, offset) \
	NPS_IO_MEM_REG_CPU(cpu,\
				NPS_MSU_BLOCK_ID,\
				offset)

#define NPS_CPU_MTM_REGS(cpu, offset) \
	NPS_IO_MEM_REG_CPU(cpu,\
			NPS_CPU_MTM_ID(NPS_CPU_TO_CORE_NUM(cpu)),\
			offset)

/* cpu registers */
#define REG_CPU_CONFIG(cpu)		NPS_CPU_MTM_REGS(cpu, 0x40)

/* MSU registers */
#define REG_CPU_RTC_LO(cpu)		NPS_CPU_MSU_REGS(cpu, 0x34)
#define REG_CPU_RTC_HI(cpu)		NPS_CPU_MSU_REGS(cpu, 0x35)

#define CPU_SEC_ENTRY_POINT	(unsigned int *)0xFFFFFFFC
#define CPU_REGS_BASE		0xC0002000
#define CPU_REGS_IPI_BASE	(CPU_REGS_BASE + 0xC0)
#define REGS_CPU_IPI(cpu)	(unsigned int *)(CPU_REGS_IPI_BASE + (cpu * 4))
#define REG_CPU_RST_CTL		(unsigned int *)(CPU_REGS_BASE + 0x14)
#define REG_CPU_HALT_CTL	(unsigned int *)(CPU_REGS_BASE + 0x18)

void __init ez_first_lines_of_secondary(void);

#endif	/* CONFIG_SMP */

#endif
