/*******************************************************************************

  Copyright(c) 2015 EZchip Technologies.

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

#ifndef _PLAT_EZNPS_CTOP_H
#define _PLAT_EZNPS_CTOP_H

#define NPS_HOST_REG_BASE		0xF6000000

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
#define CTOP_AUX_UDMC			(CTOP_AUX_BASE + 0x300)

/* Do not use D$ for address in 2G-3G */
#define HW_COMPLY_KRN_NOT_D_CACHED	(1 << 31)

#ifndef __ASSEMBLY__
#define NPS_CRG_BLKID			0x480
#define NPS_CRG_SYNC_BIT		1
#define NPS_CRG_GEN_PURP0		0x1BF

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

struct nps_host_reg_aux_udmc {
	union {
		struct {
			u32 dcp:1, cme:1, __reserved:20, nat:3,
			__reserved2:5, dcas:3;
		};
		u32 value;
	};
};

struct nps_host_reg_aux_mt_ctrl {
	union {
		struct {
			u32 mten:1, hsen:1, scd:1, sten:1,
			__reserved:4, st_cnt:4, __reserved2:8,
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

/* CRG registers */
#define REG_GEN_PURP_0	nps_host_reg_non_cl(NPS_CRG_BLKID, NPS_CRG_GEN_PURP0)
#endif /* __ASEMBLY__ */

/* EZchip core instructions */
#define CTOP_INST_SCHD_RW			0x3e6f7004
#define CTOP_INST_SCHD_RD			0x3e6f7084
#define CTOP_INST_ASRI_0_R3			0x3b56003e
#define CTOP_INST_XEX_DI_R2_R2_R3		0x4a664c00
#define CTOP_INST_EXC_DI_R2_R2_R3		0x4a664c01
#define CTOP_INST_AADD_DI_R2_R2_R3		0x4a664c02
#define CTOP_INST_AAND_DI_R2_R2_R3		0x4a664c04
#define CTOP_INST_AOR_DI_R2_R2_R3		0x4a664c05
#define CTOP_INST_AXOR_DI_R2_R2_R3		0x4a664c06
#define CTOP_INST_MOV2B_FLIP_R3_B1_B2_INST	0x5b60
#define CTOP_INST_MOV2B_FLIP_R3_B1_B2_LIMM	0x00010422

#endif /* _PLAT_EZNPS_CTOP_H */
