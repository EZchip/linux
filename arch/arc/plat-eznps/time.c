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
	unsigned long cyc;

	 __asm__ __volatile__("rtsc %0,0 \r\n" : "=r" (cyc));

	return (cycle_t) cyc;
#else /* CONFIG_SMP */
	return (cycle_t) read_aux_reg(ARC_REG_TIMER1_CNT);
#endif
}

