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

/* MTM auxiliary registers */
#define AUX_REG_MT_CTRL			(CTOP_AUX_BASE + 0x20)
#define AUX_REG_HW_COMPLY		(CTOP_AUX_BASE + 0x24)
#define AUX_REG_TSI1			(CTOP_AUX_BASE + 0x50)

/* Do not use D$ for address in 2G-3G */
#define HW_COMPLY_KRN_NOT_D_CACHED	(1 << 28)

#ifndef __ASSEMBLY__
#define NPS_CRG_BLKID			0x480
#define NPS_CRG_SYNC_BIT		1

#define NPS_GIM_BLKID				0x5c0
#define NPS_GIM_UART_LINE			(1 << 7)
#define NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE	(1 << 10)
#define NPS_GIM_DBG_LAN_EAST_RX_RDY_LINE	(1 << 11)
#define NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE	(1 << 25)
#define NPS_GIM_DBG_LAN_WEST_RX_RDY_LINE	(1 << 26)

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
			u32 dcp:1, cme:1, __reserved:19, nat:3,
			__reserved2:5, dcas:3;
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
#define REG_GIM_P_INT_DST_10	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x13a)
#define REG_GIM_P_INT_DST_11	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x13b)
#define REG_GIM_P_INT_DST_25	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x149)
#define REG_GIM_P_INT_DST_26	nps_host_reg_non_cl(NPS_GIM_BLKID, 0x14a)

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
