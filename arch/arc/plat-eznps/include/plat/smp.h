/*
* Copyright (c) 2016, Mellanox Technologies. All rights reserved.
*
* This software is available to you under a choice of one of two
* licenses.  You may choose to be licensed under the terms of the GNU
* General Public License (GPL) Version 2, available from the file
* COPYING in the main directory of this source tree, or the
* OpenIB.org BSD license below:
*
*     Redistribution and use in source and binary forms, with or
*     without modification, are permitted provided that the following
*     conditions are met:
*
*      - Redistributions of source code must retain the above
*        copyright notice, this list of conditions and the following
*        disclaimer.
*
*      - Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following
*        disclaimer in the documentation and/or other materials
*        provided with the distribution.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef __PLAT_EZNPS_SMP_H
#define __PLAT_EZNPS_SMP_H

#ifdef CONFIG_SMP
#include <linux/types.h>
#include <asm/arcregs.h>

struct global_id {
	union {
#ifdef CONFIG_EZNPS_MTM_EXT
		struct {
			u32 __reserved:20, cluster:4, core:4, thread:4;
		};
#else
		struct {
			u32 __reserved:24, cluster:4, core:4;
		};
#endif
		u32 value;
	};
};

#define IPI_IRQ						5

#define ARC_PLAT_START_PC_ALIGN				1024

#define NPS_MSU_BLKID					0x18
#define NPS_MSU_EN_CFG					0x80
#define NPS_MSU_TICK_LOW				0xC8
#define NPS_MSU_TICK_HIGH				0xC9
#define NPS_MTM_CPU_CFG					0x90

/*
 * Convert logical to physical CPU IDs
 *
 * The conversion swap bits 1 and 2 of cluster id (out of 4 bits)
 * Now quad of logical clusters id's are adjacent physicaly,
 * like can be seen in following table.
 * CPU ids are in format: logical (physical)
 *
 * 3 |  5 (3)  |  7 (7)  |  13 (11) |  15 (15)
 * 2 |  4 (2)  |  6 (6)  |  12 (10) |  14 (14)
 * 1 |  1 (1)  |  3 (5)  |  9  (9)  |  11 (13)
 * 0 |  0 (0)  |  2 (4)  |  8  (8)  |  10 (12)
 * -------------------------------------------
 *   |   0     |   1     |   2      |    3
 */
static inline int nps_cluster_logic_to_phys(int cluster)
{
	__asm__ __volatile__(
	"       mov r3,%0\n"
	"       .short %1\n"
	"       .word %2\n"
	"       mov %0,r3\n"
	: "+r"(cluster)
	: "i"(CTOP_INST_MOV2B_FLIP_R3_B1_B2_INST),
	  "i"(CTOP_INST_MOV2B_FLIP_R3_B1_B2_LIMM)
	: "r3");

	return cluster;
}

#define NPS_CPU_TO_CLUSTER_NUM(cpu) \
	({ struct global_id gid; gid.value = cpu; \
		nps_cluster_logic_to_phys(gid.cluster); })
#define NPS_CPU_TO_CORE_NUM(cpu) \
	({ struct global_id gid; gid.value = cpu; gid.core; })
#ifdef CONFIG_EZNPS_MTM_EXT
#define NPS_CPU_TO_THREAD_NUM(cpu) \
	({ struct global_id gid; gid.value = cpu; gid.thread; })
#else
#define NPS_CPU_TO_THREAD_NUM(cpu) 0
#endif

struct nps_host_reg_address {
	union {
		struct {
			u32	base:8,	cl_x:4, cl_y:4,
				blkid:6, reg:8, __reserved:2;
		};
		u32 value;
	};
};

static inline void *nps_host_reg(u32 cpu, u32 blkid, u32 reg)
{
	struct nps_host_reg_address reg_address;
	u32 cl = NPS_CPU_TO_CLUSTER_NUM(cpu);

	reg_address.value = NPS_HOST_REG_BASE;
	reg_address.cl_x  = (cl >> 2) & 0x3;
	reg_address.cl_y  = cl & 0x3;
	reg_address.blkid = blkid;
	reg_address.reg   = reg;

	return (void *)reg_address.value;
}

static inline void *nps_mtm_reg_addr(u32 cpu, u32 reg)
{
	u32 core = NPS_CPU_TO_CORE_NUM(cpu);
	u32 blkid = (((core & 0x0C) << 2) | (core & 0x03));

	return nps_host_reg(cpu, blkid, reg);
}

struct nps_host_reg_msu_en_cfg {
	union {
		struct {
			u32	__reserved1:11,
			rtc_en:1, ipc_en:1, gim_1_en:1,
			gim_0_en:1, ipi_en:1, buff_e_rls_bmuw:1,
			buff_e_alc_bmuw:1, buff_i_rls_bmuw:1, buff_i_alc_bmuw:1,
			buff_e_rls_bmue:1, buff_e_alc_bmue:1, buff_i_rls_bmue:1,
			buff_i_alc_bmue:1, __reserved2:1, buff_e_pre_en:1,
			buff_i_pre_en:1, pmuw_ja_en:1, pmue_ja_en:1,
			pmuw_nj_en:1, pmue_nj_en:1, msu_en:1;
		};
		u32 value;
	};
};

static inline void *nps_msu_reg_addr(u32 cpu, u32 reg)
{
	return nps_host_reg(cpu, NPS_MSU_BLKID, reg);
}

extern struct cpumask _cpu_possible_mask;
void eznps_init_early_smp(void);
void eznps_smp_init_cpu(unsigned int cpu);

#endif /* CONFIG_SMP */

#endif
