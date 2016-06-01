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

#include <linux/smp.h>
#include <linux/io.h>
#include <asm/arcregs.h>
#include <plat/mtm.h>

#define MAX_MTM_HS_CTR		255
#define MIN_MTM_HS_CTR		1
#define MT_CTRL_ST_CNT		0xF
#define NPS_NUM_HW_THREADS	0x10

static int mtm_hs_ctr = MAX_MTM_HS_CTR;

/* Handle an out of bounds mtm hs counter value */
static void handle_mtm_hs_ctr_out_of_bounds_error(uint8_t val)
{
	pr_err("** The value of mtm_hs_ctr is out of bounds!\n" \
		"** It must be in the range [%d,%d] (inclusive)\n" \
		"Setting mtm_hs_ctr to %d\n",
		MIN_MTM_HS_CTR,
		MAX_MTM_HS_CTR,
		val);

	mtm_hs_ctr = val;
}

/* Verify and set the value of the mtm hs counter */
static int __init set_mtm_hs_ctr(char *ctr_str)
{
	int ret;
	long hs_ctr;

	ret = kstrtol(ctr_str, 0, &hs_ctr);
	if (ret) {
		pr_err("** Error parsing the value of mtm_hs_ctr" \
			"\n** Make sure you entered a valid integer value\n" \
			"Setting mtm_hs_ctr to default value: %d\n",
			MAX_MTM_HS_CTR);
		mtm_hs_ctr = MAX_MTM_HS_CTR;
		return -EINVAL;
	}

	if (hs_ctr > MAX_MTM_HS_CTR) {
		handle_mtm_hs_ctr_out_of_bounds_error(MAX_MTM_HS_CTR);
		return -EDOM;
	}

	if (hs_ctr < MIN_MTM_HS_CTR) {
		handle_mtm_hs_ctr_out_of_bounds_error(MIN_MTM_HS_CTR);
		return -EDOM;
	}

	mtm_hs_ctr = hs_ctr;

	return 0;
}
early_param("nps_mtm_hs_ctr", set_mtm_hs_ctr);

static void mtm_init_nat(int cpu)
{
	struct nps_host_reg_mtm_cfg mtm_cfg;
	struct nps_host_reg_aux_udmc udmc;
	int log_nat, nat = 0, i, t;

	/* Iterate core threads and update nat */
	for (i = 0, t = cpu; i < NPS_NUM_HW_THREADS; i++, t++)
		nat += test_bit(t, cpumask_bits(&_cpu_possible_mask));

	switch (nat) {
	case 1:
		log_nat = 0;
		break;
	case 2:
		log_nat = 1;
		break;
	case 4:
		log_nat = 2;
		break;
	case 8:
		log_nat = 3;
		break;
	case 16:
		log_nat = 4;
		break;
	default:
		pr_warn("BUG: got non valid NAT %d!\n", nat);
		log_nat = 0;
		break;
	}

	udmc.value = read_aux_reg(CTOP_AUX_UDMC);
	udmc.nat = log_nat;
	write_aux_reg(CTOP_AUX_UDMC, udmc.value);

	mtm_cfg.value = ioread32be(MTM_CFG(cpu));
	mtm_cfg.nat = log_nat;
	iowrite32be(mtm_cfg.value, MTM_CFG(cpu));
}

static void mtm_init_thread(int cpu)
{
	int i, tries = 5;
	struct nps_host_reg_thr_init thr_init;
	struct nps_host_reg_thr_init_sts thr_init_sts;

	/* Set thread init register */
	thr_init.value = 0;
	iowrite32be(thr_init.value, MTM_THR_INIT(cpu));
	thr_init.thr_id = NPS_CPU_TO_THREAD_NUM(cpu);
	thr_init.str = 1;
	iowrite32be(thr_init.value, MTM_THR_INIT(cpu));

	/* Poll till thread init is done */
	for (i = 0; i < tries; i++) {
		thr_init_sts.value = ioread32be(MTM_THR_INIT_STS(cpu));
		if (thr_init_sts.thr_id == thr_init.thr_id) {
			if (thr_init_sts.bsy)
				continue;
			else if (thr_init_sts.err)
				pr_warn("Failed to thread init cpu %u\n", cpu);
			break;
		}

		pr_warn("Wrong thread id in thread init for cpu %u\n", cpu);
		break;
	}

	if (i == tries)
		pr_warn("Got thread init timeout for cpu %u\n", cpu);
}

int mtm_enable_thread(int cpu)
{
	struct nps_host_reg_mtm_cfg mtm_cfg;

	if (NPS_CPU_TO_THREAD_NUM(cpu) == 0)
		return 1;

	/* Enable thread in mtm */
	mtm_cfg.value = ioread32be(MTM_CFG(cpu));
	mtm_cfg.ten |= (1 << (NPS_CPU_TO_THREAD_NUM(cpu)));
	iowrite32be(mtm_cfg.value, MTM_CFG(cpu));

	return 0;
}

void mtm_enable_core(unsigned int cpu)
{
	int i;
	struct nps_host_reg_aux_mt_ctrl mt_ctrl;
	struct nps_host_reg_mtm_cfg mtm_cfg;

	if (NPS_CPU_TO_THREAD_NUM(cpu) != 0)
		return;

	/* Initialize Number of Active Threads */
	mtm_init_nat(cpu);

	/* Initialize mtm_cfg */
	mtm_cfg.value = ioread32be(MTM_CFG(cpu));
	mtm_cfg.ten = 1;
	iowrite32be(mtm_cfg.value, MTM_CFG(cpu));

	/* Initialize all other threads in core */
	for (i = 1; i < NPS_NUM_HW_THREADS; i++)
		mtm_init_thread(cpu + i);

	/* Enable HW schedule, stall counter, mtm */
	mt_ctrl.value = 0;
	mt_ctrl.hsen = 1;
	mt_ctrl.hs_cnt = mtm_hs_ctr;
	mt_ctrl.mten = 1;
	write_aux_reg(AUX_REG_MT_CTRL, mt_ctrl.value);

	/*
	 * HW scheduling mechanism will start working
	 * Only after call to instruction "schd.rw".
	 * cpu_relax() calls "schd.rw" instruction.
	 */
	cpu_relax();
}
