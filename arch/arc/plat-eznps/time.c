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
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <asm/clk.h>
#include <asm/mtm.h>
#include <plat/smp.h>
#include <plat/mtm.h>

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
	void *tick_low, *tick_high;

	local_irq_save(flags);

	cpu = smp_processor_id();

	tick_low = nps_msu_reg_addr(cpu, NPS_MSU_TICK_LOW);
	tick_high = nps_msu_reg_addr(cpu, NPS_MSU_TICK_HIGH);

	/* get correct clk value */
	do {
		/* MSW */
		tick.clk_value[0] = ioread32be(tick_high);
		/* LSW */
		tick.clk_value[1] = ioread32be(tick_low);
		cmp_value = ioread32be(tick_high);
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

#ifdef CONFIG_EZNPS_SHARED_TIMER
void eznps_timer_event_setup(unsigned int cycles)
{
	write_aux_reg(ARC_REG_TIMER0_LIMIT, cycles);
	write_aux_reg(ARC_REG_TIMER0_CNT, 0);	/* start from 0 */

	write_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE | TIMER_CTRL_NH);
}

static void eznps_rm_thread(bool remove_thread)
{
	unsigned int cflags;
	unsigned int enabled_threads;
	unsigned long flags;
	int thread;

	local_irq_save(flags);
	hw_schd_save(&cflags);

	enabled_threads = read_aux_reg(AUX_REG_TSI1);

	/* remove thread from TSI1 */
	if (remove_thread) {
		thread = read_aux_reg(CTOP_AUX_THREAD_ID);
		enabled_threads &= ~(1 << thread);
		write_aux_reg(AUX_REG_TSI1, enabled_threads);
	}

	/* Re-arm the timer if needed */
	if (!enabled_threads)
		write_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_NH);
	else
		write_aux_reg(ARC_REG_TIMER0_CTRL,
				TIMER_CTRL_IE | TIMER_CTRL_NH);

	hw_schd_restore(cflags);
	local_irq_restore(flags);
}

static void eznps_add_thread(bool set_event)
{
	int thread;
	unsigned int cflags, enabled_threads;
	unsigned long flags;

	local_irq_save(flags);
	hw_schd_save(&cflags);

	/* add thread to TSI1 */
	thread = read_aux_reg(CTOP_AUX_THREAD_ID);
	enabled_threads = read_aux_reg(AUX_REG_TSI1);
	enabled_threads |= (1 << thread);
	write_aux_reg(AUX_REG_TSI1, enabled_threads);

	/* set next timer event */
	if (set_event)
		write_aux_reg(ARC_REG_TIMER0_CTRL,
				TIMER_CTRL_IE | TIMER_CTRL_NH);

	hw_schd_restore(cflags);
	local_irq_restore(flags);
}

static void eznps_timer_mask(void)
{
	unsigned int ienb;

	ienb = read_aux_reg(AUX_IENABLE);
	ienb &= ~(1 << TIMER0_IRQ);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void eznps_timer_unmask(void)
{
	unsigned int ienb;

	ienb = read_aux_reg(AUX_IENABLE);
	ienb |= (1 << TIMER0_IRQ);
	write_aux_reg(AUX_IENABLE, ienb);
}

static int eznps_clkevent_set_next_event(unsigned long delta,
		struct clock_event_device *dev)
{
	eznps_add_thread(true);
	eznps_timer_unmask();

	return 0;
}

/*
 * Whenever anyone tries to change modes, we just mask interrupts
 * and wait for the next event to get set.
 */
static int eznps_clkevent_timer_shutdown(struct clock_event_device *dev)
{
	eznps_timer_mask();

	return 0;
}


static int eznps_clkevent_set_periodic(struct clock_event_device *dev)
{
	eznps_add_thread(false);
	if (read_aux_reg(CTOP_AUX_THREAD_ID) == 0)
		eznps_timer_event_setup(arc_get_core_freq() / HZ);

	return 0;
}

static int eznps_clkevent_set_oneshot(struct clock_event_device *dev)
{
	eznps_rm_thread(true);
	eznps_clkevent_timer_shutdown(dev);

	return 0;
}

static DEFINE_PER_CPU(struct clock_event_device, nps_clockevent_device) = {
	.name		= "ARC Timer0",
	.features	= CLOCK_EVT_FEAT_ONESHOT |
			  CLOCK_EVT_FEAT_PERIODIC,
	.rating		= 300,
	.irq		= TIMER0_IRQ,	/* hardwired, no need for resources */
	.set_next_event = eznps_clkevent_set_next_event,
	.set_state_shutdown = eznps_clkevent_timer_shutdown,
	.set_state_periodic	= eznps_clkevent_set_periodic,
	.set_state_oneshot = eznps_clkevent_set_oneshot,
	.set_state_oneshot_stopped = eznps_clkevent_timer_shutdown,
	.tick_resume = eznps_clkevent_timer_shutdown,
};

static irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
	/*
	 * Note that generic IRQ core could have passed @evt for @dev_id if
	 * irq_set_chip_and_handler() asked for handle_percpu_devid_irq()
	 */
	struct clock_event_device *evt = this_cpu_ptr(&nps_clockevent_device);
	int irq_reenable = clockevent_state_periodic(evt);

	eznps_rm_thread(!irq_reenable);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

/*
 * Setup the local event timer for @cpu
 */
void arc_local_timer_setup(void)
{
	struct clock_event_device *evt = this_cpu_ptr(&nps_clockevent_device);
	int cpu = smp_processor_id();

	evt->cpumask = cpumask_of(cpu);
	clockevents_config_and_register(evt, arc_get_core_freq(),
					0, ARC_TIMER_MAX);

	/* setup the per-cpu timer IRQ handler - for all cpus */
	arc_request_percpu_irq(TIMER0_IRQ, cpu, timer_irq_handler,
				"Timer0 (per-cpu-tick)", evt);
}

#endif /* CONFIG_EZNPS_MTM_SHARED_TIMER */
