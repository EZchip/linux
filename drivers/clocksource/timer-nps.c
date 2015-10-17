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

#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <plat/ctop.h>

#define NPS_MSU_TICK_LOW	0xC8
#define NPS_CLUSTER_OFFSET	8
#define NPS_CLUSTER_NUM		16

static void *nps_msu_reg_low_addr[NPS_CLUSTER_NUM] __read_mostly;

/*
 * To get the value from the Global Timer Counter register proceed as follows:
 * 1. Read the upper 32-bit timer counter register
 * 2. Read the lower 32-bit timer counter register
 * 3. Read the upper 32-bit timer counter register again. If the value is
 *  different to the 32-bit upper value read previously, go back to step 2.
 *  Otherwise the 64-bit timer counter value is correct.
 */
static cycle_t nps_clksrc_read(struct clocksource *clksrc)
{
	u64 counter;
	u32 lower, upper, old_upper;
	void *lower_p, *upper_p;
	int cluster = (smp_processor_id() >> NPS_CLUSTER_OFFSET);

	lower_p = nps_msu_reg_low_addr[cluster];
	upper_p = lower_p + 4;

	upper = ioread32be(upper_p);
	do {
		old_upper = upper;
		lower = ioread32be(lower_p);
		upper = ioread32be(upper_p);
	} while (upper != old_upper);

	counter = (upper << 32) | lower;
	return (cycle_t)counter;
}

static struct clocksource nps_counter = {
	.name	= "EZnps-tick",
	.rating = 301,
	.read   = nps_clksrc_read,
	.mask   = CLOCKSOURCE_MASK(64),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init nps_setup_clocksource(struct device_node *node)
{
	struct clocksource *clksrc = &nps_counter;
	unsigned long rate, dt_root;
	int ret, cluster;

	for (cluster = 0; cluster < NPS_CLUSTER_NUM; cluster++)
		nps_msu_reg_low_addr[cluster] =
			nps_host_reg((cluster << NPS_CLUSTER_OFFSET),
				 NPS_MSU_BLKID, NPS_MSU_TICK_LOW);

	dt_root = of_get_flat_dt_root();
	rate = (u32)of_get_flat_dt_prop(dt_root, "clock-frequency", NULL);

	ret = clocksource_register_hz(clksrc, rate);
	if (ret)
		pr_err("Couldn't register clock source.\n");
}

CLOCKSOURCE_OF_DECLARE(nps_400, "nps,400-timer",
		       nps_setup_clocksource);
