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

#include <linux/init.h>
#include <linux/io.h>
#include <asm/mach_desc.h>
#include <plat/mtm.h>

static void __init eznps_configure_msu(void)
{
	int cpu;
	struct nps_host_reg_msu_en_cfg msu_en_cfg = {.value = 0};

	msu_en_cfg.msu_en = 1;
	msu_en_cfg.ipi_en = 1;
	msu_en_cfg.gim_0_en = 1;
	msu_en_cfg.gim_1_en = 1;

	/* enable IPI and GIM messages on all clusters */
	for (cpu = 0 ; cpu < eznps_max_cpus; cpu += eznps_cpus_per_cluster)
		iowrite32be(msu_en_cfg.value,
			    nps_host_reg(cpu, NPS_MSU_BLKID, NPS_MSU_EN_CFG));
}

static void __init eznps_configure_gim(void)
{
	u32 reg_value;
	u32 gim_int_lines;
	struct nps_host_reg_gim_p_int_dst gim_p_int_dst = {.value = 0};

	/* adding the low lines to the interrupt lines to set polarity */
	gim_int_lines = NPS_GIM_UART_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_EAST_RX_RDY_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE;
	gim_int_lines |= NPS_GIM_DBG_LAN_WEST_RX_RDY_LINE;

	/*
	 * IRQ polarity
	 * low or high level
	 * negative or positive edge
	 */
	reg_value = ioread32be(REG_GIM_P_INT_POL_0);
	reg_value &= ~gim_int_lines;
	iowrite32be(reg_value, REG_GIM_P_INT_POL_0);

	/* IRQ type level or edge */
	reg_value = ioread32be(REG_GIM_P_INT_SENS_0);
	reg_value |= NPS_GIM_DBG_LAN_EAST_TX_DONE_LINE;
	reg_value |= NPS_GIM_DBG_LAN_WEST_TX_DONE_LINE;
	iowrite32be(reg_value, REG_GIM_P_INT_SENS_0);

	/*
	 * GIM interrupt select type for
	 * dbg_lan TX and RX interrupts
	 * should be type 1
	 * type 0 = IRQ line 6
	 * type 1 = IRQ line 7
	 */
	gim_p_int_dst.is = 1;
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_10);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_11);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_25);
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_26);

	/* adding watchdog interrupt to the interrupt lines */
	gim_int_lines |= NPS_GIM_WDOG_LINE;

	/*
	 * CTOP IRQ lines should be defined
	 * as blocking in GIM
	*/
	iowrite32be(gim_int_lines, REG_GIM_P_INT_BLK_0);

	/* watchdog interrupt should be sent to the interrupt out line */
	gim_p_int_dst.value = 0;
	gim_p_int_dst.int_out_en = 1;
	iowrite32be(gim_p_int_dst.value, REG_GIM_P_INT_DST_0);
	iowrite32be(1, REG_GIM_IO_INT_EN);

	/* enable CTOP IRQ lines in GIM */
	iowrite32be(gim_int_lines, REG_GIM_P_INT_EN_0);
}

static void __init eznps_early_init(void)
{
	eznps_configure_msu();
	eznps_configure_gim();
}

static const char *eznps_compat[] __initconst = {
	"ezchip,arc-nps",
	NULL,
};

MACHINE_START(NPS, "nps")
	.dt_compat	= eznps_compat,
	.init_early	= eznps_early_init,
MACHINE_END
