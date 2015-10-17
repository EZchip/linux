/*
 * Copyright(c) 2015 EZchip Technologies.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/cpu.h>
#include <soc/nps/common.h>

#define NPS_MSU_TICK_LOW	0xC8
#define NPS_CLUSTER_OFFSET	8
#define NPS_CLUSTER_NUM		16

/* This array is per cluster of CPUs (Each NPS400 cluster got 256 CPUs) */
static void *nps_msu_reg_low_addr[NPS_CLUSTER_NUM] __read_mostly;

static unsigned long nps_timer_rate;

static cycle_t nps_clksrc_read(struct clocksource *clksrc)
{
	int cluster = raw_smp_processor_id() >> NPS_CLUSTER_OFFSET;

	return (cycle_t)ioread32be(nps_msu_reg_low_addr[cluster]);
}

static struct clocksource nps_counter = {
	.name	= "EZnps-tick",
	.rating = 301,
	.read   = nps_clksrc_read,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init nps_setup_clocksource(struct device_node *node,
					 struct clk *clk)
{
	struct clocksource *clksrc = &nps_counter;
	int ret, cluster;

	for (cluster = 0; cluster < NPS_CLUSTER_NUM; cluster++)
		nps_msu_reg_low_addr[cluster] =
			nps_host_reg((cluster << NPS_CLUSTER_OFFSET),
				 NPS_MSU_BLKID, NPS_MSU_TICK_LOW);

	ret = clk_prepare_enable(clk);
	if (ret)
		pr_err("Couldn't enable parent clock\n");

	nps_timer_rate = clk_get_rate(clk);

	ret = clocksource_register_hz(clksrc, nps_timer_rate);
	if (ret)
		pr_err("Couldn't register clock source.\n");
}

static void __init nps_timer_init(struct device_node *node)
{
	struct clk *clk;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk))
		panic("Can't get timer clock");

	nps_setup_clocksource(node, clk);
}

CLOCKSOURCE_OF_DECLARE(ezchip_nps400_clksrc, "ezchip,nps400-timer",
		       nps_timer_init);
