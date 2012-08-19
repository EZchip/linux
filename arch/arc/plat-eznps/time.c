/*******************************************************************************

  EZNPS Platform time
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

#include <linux/clocksource.h>
#include <asm/arcregs.h>
#ifdef CONFIG_SMP
#include <linux/smp.h>
#include <asm/setup.h>
#include <plat/smp.h>

unsigned long long rtc64(unsigned int cpu)
{
	union RTC_64 rtc_64;
	unsigned long cmpValue;

	/* get correct clk value */
	do {
		/* MSW */
		rtc_64.clk_value[0] = __raw_readl(REG_CPU_RTC_HI(cpu));
		/* LSW */
		rtc_64.clk_value[1] = __raw_readl(REG_CPU_RTC_LO(cpu));
		cmpValue = __raw_readl(REG_CPU_RTC_HI(cpu));
	} while (rtc_64.clk_value[0] != cmpValue);

	return rtc_64.retValue;
}
#endif

void arc_platform_timer_init(void)
{

}

void arc_platform_periodic_timer_setup(unsigned int limit)
{
    /* setup start and end markers */
	write_aux_reg(ARC_REG_TIMER0_LIMIT, limit);
	write_aux_reg(ARC_REG_TIMER0_CNT, 0);   /* start from 0 */

    /* IE: Interrupt on count = limit,
     * NH: Count cycles only when CPU running (NOT Halted)
     */
	write_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE | TIMER_CTRL_NH);
}

cycle_t arc_platform_read_timer1(struct clocksource *cs)
{
#ifdef CONFIG_SMP
	if (!running_on_hw)
		return (cycle_t) (*(u32 *)0xC0002024);
	else
	{
		extern unsigned long eznps_he_version;
		if (eznps_he_version == 1)
		{
			return (cycle_t) rtc64(smp_processor_id());
		}
		else
		{
			unsigned long cyc;

			 __asm__ __volatile__("rtsc %0,0 \r\n" : "=r" (cyc));

			return (cycle_t) cyc;
		}
	}
#else /* CONFIG_SMP */
	return (cycle_t) read_aux_reg(ARC_REG_TIMER1_CNT);
#endif
}

