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

#include <linux/smp.h>
#include <linux/clocksource.h>
#include <linux/io.h>
#include <asm/clk.h>
#include <plat/smp.h>

union Tick64 {
	u64 ret_value;
	u32 clk_value[2];
};

static cycle_t eznps_counter_read(struct clocksource *cs)
{
	union Tick64 tick;
	unsigned long cmp_value;
	unsigned int cpu;
	unsigned long flags;

	local_irq_save(flags);

	cpu = smp_processor_id();

	/* get correct clk value */
	do {
		void *low  = nps_msu_reg_addr(cpu, NPS_MSU_TICK_LOW),
		     *high = nps_msu_reg_addr(cpu, NPS_MSU_TICK_HIGH);

		/* MSW */
		tick.clk_value[0] = ioread32be(high);
		/* LSW */
		tick.clk_value[1] = ioread32be(low);
		cmp_value = ioread32be(high);
	} while (tick.clk_value[0] != cmp_value);

	local_irq_restore(flags);

	return tick.ret_value;
}

static struct clocksource eznps_counter = {
	.name	= "EZnps tick",
	.rating	= 301,
	.read	= eznps_counter_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

void eznps_counter_setup(void)
{
	clocksource_register_hz(&eznps_counter, arc_get_core_freq());
}
