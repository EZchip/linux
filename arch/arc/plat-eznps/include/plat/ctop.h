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

#ifndef _PLAT_EZNPS_CTOP_H
#define _PLAT_EZNPS_CTOP_H

#define pr_err_with_indent(str)			\
	pr_info("          " str)
#define pr_err_fmt_with_indent(format, params)	\
	pr_info("          " format, params)

#define NPS_HOST_REG_BASE		0xF6000000

/* virtual addresses */
#define CMEM_BASE			0x57F00000
#define CMEM_SHARED_BASE		0x57F08000
#define CMEM_SIZE			(1024 * 16)

/* core auxiliary registers */
#define CTOP_AUX_BASE			0xFFFFF800
#define CTOP_AUX_GLOBAL_ID		(CTOP_AUX_BASE + 0x000)
#define CTOP_AUX_CLUSTER_ID		(CTOP_AUX_BASE + 0x004)
#define CTOP_AUX_CORE_ID		(CTOP_AUX_BASE + 0x008)
#define CTOP_AUX_THREAD_ID		(CTOP_AUX_BASE + 0x00C)
#define CTOP_AUX_LOGIC_GLOBAL_ID	(CTOP_AUX_BASE + 0x010)
#define CTOP_AUX_LOGIC_CLUSTER_ID	(CTOP_AUX_BASE + 0x014)
#define CTOP_AUX_LOGIC_CORE_ID		(CTOP_AUX_BASE + 0x018)
#define CTOP_AUX_HW_COMPLY		(CTOP_AUX_BASE + 0x024)
#define CTOP_AUX_DPC			(CTOP_AUX_BASE + 0x02C)
#define CTOP_AUX_LPC			(CTOP_AUX_BASE + 0x030)
#define CTOP_AUX_EFLAGS			(CTOP_AUX_BASE + 0x080)
#define CTOP_AUX_IACK			(CTOP_AUX_BASE + 0x088)
#define CTOP_AUX_GPA1			(CTOP_AUX_BASE + 0x08C)
#define CTOP_FMTP_BASE			(CTOP_AUX_BASE + 0x0C0)
#define CTOP_AUX_UDMC			(CTOP_AUX_BASE + 0x300)
#define CTOP_AUX_CMPC			(CTOP_AUX_BASE + 0x304)

/* core auxiliary registers properties */
#define CTOP_AUX_UDMC_DCAS_MAX_VALUE	5
#define CTOP_AUX_UDMC_NAT_MAX_VALUE	4

/* MTM auxiliary registers */
#define AUX_REG_MT_CTRL			(CTOP_AUX_BASE + 0x20)
#define AUX_REG_HW_COMPLY		(CTOP_AUX_BASE + 0x24)
#define AUX_REG_TSI1			(CTOP_AUX_BASE + 0x50)

/* MTM registers */
#define NPS_MTM_MSID_CFG_BASE				0xA0

/* CIU registers */
#define NPS_CIU_WEST_BLOCK_ID				0x8
#define NPS_CIU_EAST_BLOCK_ID				0x28
#define NPS_CIU_FMT_MSID_CFG_BASE			0x86
#define NPS_CIU_ERR_CAP_1				0xD0
#define NPS_CIU_ERR_CAP_2				0xD1
#define NPS_CIU_ERR_STS					0xD2
#define NPS_CLUSTER_MIDDLE_CORE_NUMBER			8
#define ERR_CAP_2_ADDRESS_SHIFT_LEFT_VALUE		2
#define ERR_CAP_1_SHIFT_LEFT_VALUE			2
#define ERR_CAP_1_SHIFT_RIGHT_VALUE			30

/* cluster blocks macros */

/*
 * There are 16 MTM blocks in a cluster, with the block ids 0x0, 0x1, 0x2, 0x3,
 * 0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33
 * respectively to the cores in the cluster (i.e. core 0 accesses the MTM with
 * id 0x0, core 5 accesses the MTM with id 0x11 etc...)
 */
#define NPS_CPU_TO_MTM_ID(cpu) \
	(((NPS_CPU_TO_CORE_NUM(cpu) / 4) << 4) + \
		NPS_CPU_TO_CORE_NUM(cpu) % 4)

/*
 * There are two CIU blocks in a cluster, and so cores 0-7 access the west one
 * and core 8-15 access the east one
 */
#define NPS_CPU_TO_CIU_ID(cpu) \
	(NPS_CPU_TO_CORE_NUM(cpu) < NPS_CLUSTER_MIDDLE_CORE_NUMBER ? \
	NPS_CIU_WEST_BLOCK_ID : NPS_CIU_EAST_BLOCK_ID)

/* Do not use D$ for address in 2G-3G */
#define HW_COMPLY_KRN_NOT_D_CACHED	(1 << 28)

#ifndef __ASSEMBLY__
#define NPS_CRG_BLKID			0x480
#define NPS_CRG_SYNC_BIT		1

#define NPS_GIM_BLKID				0x5c0
#define NPS_GIM_WDOG_LINE			(1 << 0)
#define NPS_GIM_UART_LINE			(1 << 7)
#define NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE	(1 << 10)
#define NPS_GIM_DBG_LAN_EAST_RX_RDY_LINE	(1 << 11)
#define NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE	(1 << 25)
#define NPS_GIM_DBG_LAN_WEST_RX_RDY_LINE	(1 << 26)

/* CMEM size macros */

/*
 * cmpc is an auxiliary register that contains the number of cmem blocks in
 * possesion of the core: the nlb field represents the number of 128 byte
 * blocks, the nmb field represents the number of 64 byte blocks and the nsb
 * field represents the number of 32 byte blocks
 */
#define GET_PRIVATE_CMEM_SIZE(cmpc) \
	(cmpc.nlb * 128 + cmpc.nmb * 64 + cmpc.nsb * 32)

#define GET_SHARED_CMEM_SIZE(private_size, dcache_size, nat) \
	(CMEM_SIZE - dcache_size - private_size * nat)

/*
 * the dcas field in the udmc auxiliary register represents the size of the
 * data cache in 1KB granularity
 */
#define GET_DATA_CACHE_SIZE(udmc) \
	((1 << (udmc.dcas - 1)) * 1024)

#define GET_ACTIVE_THREADS(udmc) \
	(1 << udmc.nat)

struct nps_host_reg_fmtp {
	union {
		struct {
			u32 v:1, c:1, uw:1, reserved28_11:11, seq:2,
			reserved15_4:12, msid:4;
		};
		u32 value;
	};
};

struct nps_host_reg_mtm_cfg {
	union {
		struct {
			u32 gen:1, gdis:1, clk_gate_dis:1, asb:1,
			__reserved:9, nat:3, ten:16;
		};
		u32 value;
	};
};

struct nps_host_reg_mtm_cpu_cfg {
	union {
		struct {
			u32 csa:22, dmsid:6, __reserved:3, cs:1;
		};
		u32 value;
	};
};

struct nps_host_reg_thr_init {
	union {
		struct {
			u32 str:1, __reserved:27, thr_id:4;
		};
		u32 value;
	};
};

struct nps_host_reg_thr_init_sts {
	union {
		struct {
			u32 bsy:1, err:1, __reserved:26, thr_id:4;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_dpc {
	union {
		struct {
			u32 ien:1, men:1, hen:1, reserved:29;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_udmc {
	union {
		struct {
			u32 dcp:1, cme:1, __reserved:19, nat:3,
			__reserved2:5, dcas:3;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_cmpc {
	union {
		struct {
			u32 reserved31:1, nlb:7, reserved23_20:4,
			nmb:8, reserved11_8:4, nsb:8;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_mt_ctrl {
	union {
		struct {
			u32 mten:1, hsen:1, scd:1, sten:1,
			st_cnt:8, __reserved2:8,
			hs_cnt:8, __reserved3:4;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_hw_comply {
	union {
		struct {
			u32 me:1, le:1, te:1, knc:1, __reserved:28;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_lpc {
	union {
		struct {
			u32 mep:1, __reserved:31;
		};
		u32 value;
	};
};

struct nps_host_reg_address_non_cl {
	union {
		struct {
			u32 base:7, blkid:11, reg:12, __reserved:2;
		};
		u32 value;
	};
};

static inline void *nps_host_reg_non_cl(u32 blkid, u32 reg)
{
	struct nps_host_reg_address_non_cl reg_address;

	reg_address.value = NPS_HOST_REG_BASE;
	reg_address.blkid = blkid;
	reg_address.reg = reg;

	return (void *)reg_address.value;
}

/* MTM registers */
struct nps_mtm_msid_cfg0_1 {
	union {
		struct {
			u32 en:1, reserved30_29:2, wp:1,
			reserved27_23:5, ims22_20:3, reserved19_12:8,
			bad11_1:11, reserved0:1;
		};
		u32 value;
	};
};

struct nps_mtm_msid_cfg2_5 {
	union {
		struct {
			u32 en:1, reserved30_29:2, wp:1,
			reserved27_24:4, ims:11, reserved12:1,
			bad:11, reserved0:1;
		};
		u32 value;
	};
};

/* CIU registers */
struct nps_ciu_err_cap_2 {
	union {
		struct {
			u32 emem_bits:2, msid:6, erc:4, rqtc:4, tt:12,
			reserved:2, addr:2;
		};
		u32 value;
	};
};

struct nps_ciu_fmt_msid_cfg {
	union {
		struct {
			u32 en:1, def_en:1, reserved29:1, wp:1,
			reserved27_24:4, ims:11, bad:11, reserved0:1;
		};
		u32 value;
	};
};

struct nps_ciu_fmt_msid_cfg_15 {
	union {
		struct {
			u32 en:1, def_en:1, reserved29:1, wp:1,
			msid:8, l2_bp:1, gs:3, nt:2, sd:2, size:12;
		};
		u32 value;
	};
};

/* CRG registers */
#define REG_GEN_PURP_0	nps_host_reg_non_cl(NPS_CRG_BLKID, 0x1bf)

/* GIM registers */
struct nps_host_reg_gim_p_int_dst {
	union {
		struct {
			u32 int_out_en:1, __reserved1:4,
			is:1, intm:2, __reserved2:4,
			nid:4, __reserved3:4, cid:4,
			__reserved4:4, tid:4;
		};
		u32 value;
	};
};

#define REG_GIM_P_INT_EN_0	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x100)
#define REG_GIM_P_INT_POL_0	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x110)
#define REG_GIM_P_INT_SENS_0	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x114)
#define REG_GIM_P_INT_BLK_0	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x118)
#define REG_GIM_P_INT_DST_0	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x130)
#define REG_GIM_P_INT_DST_10	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x13a)
#define REG_GIM_P_INT_DST_11	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x13b)
#define REG_GIM_P_INT_DST_25	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x149)
#define REG_GIM_P_INT_DST_26	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x14a)
#define REG_GIM_IO_INT_EN	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x800)

int provide_nps_mapping_information(unsigned long);
int print_memory_exception(void);
void eznps_plat_irq_init(void);

#endif /* __ASEMBLY__ */

/* EZchip core instructions */
#define CTOP_INST_HWSCHD_OFF_R3			0x3b6f00bf
#define CTOP_INST_HWSCHD_OFF_R4			0x3c6f00bf
#define CTOP_INST_HWSCHD_RESTORE_R3		0x3e6f70c3
#define CTOP_INST_HWSCHD_RESTORE_R4		0x3e6f7103
#define CTOP_INST_SCHD_RW			0x3e6f7004
#define CTOP_INST_SCHD_RD			0x3e6f7084
#define CTOP_INST_ASRI_0_R3			0x3b56003e
#define CTOP_INST_RSPI_GIC_0_R12		0x3c56117e
#define CTOP_INST_XEX_DI_R2_R2_R3		0x4a664c00
#define CTOP_INST_EXC_DI_R2_R2_R3		0x4a664c01
#define CTOP_INST_AADD_DI_R2_R2_R3		0x4a664c02
#define CTOP_INST_AAND_DI_R2_R2_R3		0x4a664c04
#define CTOP_INST_AOR_DI_R2_R2_R3		0x4a664c05
#define CTOP_INST_AXOR_DI_R2_R2_R3		0x4a664c06
#define CTOP_INST_MOV2B_FLIP_R3_B1_B2_INST	0x5b60
#define CTOP_INST_MOV2B_FLIP_R3_B1_B2_LIMM	0x00010422

#endif /* _PLAT_EZNPS_CTOP_H */
