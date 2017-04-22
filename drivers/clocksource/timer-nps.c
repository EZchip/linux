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

static unsigned long nps_timer1_freq;

static cycle_t nps_clksrc_read(struct clocksource *clksrc)
{
	int cluster = raw_smp_processor_id() >> NPS_CLUSTER_OFFSET;

	return (cycle_t)ioread32be(nps_msu_reg_low_addr[cluster]);
}

static int __init nps_setup_clocksource(struct device_node *node)
{
	struct clk *clk;
	int ret, cluster;

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("Can't get timer clock.\n");
		return PTR_ERR(clk);
	}

	for (cluster = 0; cluster < NPS_CLUSTER_NUM; cluster++)
		nps_msu_reg_low_addr[cluster] =
			nps_host_reg((cluster << NPS_CLUSTER_OFFSET),
				 NPS_MSU_BLKID, NPS_MSU_TICK_LOW);

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("Couldn't enable parent clock\n");
		return ret;
	}

	nps_timer1_freq = clk_get_rate(clk);

	ret = clocksource_mmio_init(nps_msu_reg_low_addr, "nps-tick",
				    nps_timer1_freq, 301, 32, nps_clksrc_read);
	if (ret) {
		pr_err("Couldn't register clock source.\n");
		clk_disable_unprepare(clk);
	}

	return ret;
}

CLOCKSOURCE_OF_DECLARE(ezchip_nps400_clksrc, "ezchip,nps400-timer1",
		       nps_setup_clocksource);

#ifdef CONFIG_EZNPS_MTM_EXT
#include <soc/nps/mtm.h>

/* Timer related Aux registers */
#define AUX_REG_TIMER0_TSI	0xFFFFF850	/* timer 0 HW threads mask */
#define NPS_REG_TIMER0_LIMIT	0x23		/* timer 0 limit */
#define NPS_REG_TIMER0_CTRL	0x22		/* timer 0 control */
#define NPS_REG_TIMER0_CNT	0x21		/* timer 0 count */

#define TIMER0_CTRL_IE	(1 << 0) /* Interrupt when Count reaches limit */
#define TIMER0_CTRL_NH	(1 << 1) /* Count only when CPU NOT halted */

#define NPS_TIMER_MAX	0xFFFFFFFF

static unsigned long nps_timer0_freq;
static unsigned long nps_timer0_irq;

/*
 * Arm the timer to interrupt after @cycles
 */
static void nps_clkevent_timer_event_setup(unsigned int cycles)
{
	write_aux_reg(NPS_REG_TIMER0_LIMIT, cycles);
	write_aux_reg(NPS_REG_TIMER0_CNT, 0);   /* start from 0 */

	write_aux_reg(NPS_REG_TIMER0_CTRL, TIMER0_CTRL_IE | TIMER0_CTRL_NH);
}

static void nps_clkevent_rm_thread(bool remove_thread)
{
	unsigned int cflags;
	unsigned int enabled_threads;
	unsigned long flags;
	int thread;

	local_irq_save(flags);
	hw_schd_save(&cflags);

	enabled_threads = read_aux_reg(AUX_REG_TIMER0_TSI);

	/* remove thread from TSI1 */
	if (remove_thread) {
		thread = read_aux_reg(CTOP_AUX_THREAD_ID);
		enabled_threads &= ~(1 << thread);
		write_aux_reg(AUX_REG_TIMER0_TSI, enabled_threads);
	}

	/* Re-arm the timer if needed */
	if (!enabled_threads)
		write_aux_reg(NPS_REG_TIMER0_CTRL, TIMER0_CTRL_NH);
	else
		write_aux_reg(NPS_REG_TIMER0_CTRL,
			      TIMER0_CTRL_IE | TIMER0_CTRL_NH);

	hw_schd_restore(cflags);
	local_irq_restore(flags);
}

static void nps_clkevent_add_thread(bool set_event)
{
	int thread;
	unsigned int cflags, enabled_threads;
	unsigned long flags;

	local_irq_save(flags);
	hw_schd_save(&cflags);

	/* add thread to TSI1 */
	thread = read_aux_reg(CTOP_AUX_THREAD_ID);
	enabled_threads = read_aux_reg(AUX_REG_TIMER0_TSI);
	enabled_threads |= (1 << thread);
	write_aux_reg(AUX_REG_TIMER0_TSI, enabled_threads);

	/* set next timer event */
	if (set_event)
		write_aux_reg(NPS_REG_TIMER0_CTRL,
			      TIMER0_CTRL_IE | TIMER0_CTRL_NH);

	hw_schd_restore(cflags);
	local_irq_restore(flags);
}

static int nps_clkevent_set_next_event(unsigned long delta,
				       struct clock_event_device *dev)
{
	struct irq_desc *desc = irq_to_desc(nps_timer0_irq);
	struct irq_chip *chip = irq_data_get_irq_chip(&desc->irq_data);

	nps_clkevent_add_thread(true);
	chip->irq_unmask(&desc->irq_data);

	return 0;
}

/*
 * Whenever anyone tries to change modes, we just mask interrupts
 * and wait for the next event to get set.
 */
static int nps_clkevent_timer_shutdown(struct clock_event_device *dev)
{
	struct irq_desc *desc = irq_to_desc(nps_timer0_irq);
	struct irq_chip *chip = irq_data_get_irq_chip(&desc->irq_data);

	chip->irq_mask(&desc->irq_data);

	return 0;
}

static int nps_clkevent_set_periodic(struct clock_event_device *dev)
{
	nps_clkevent_add_thread(false);
	if (read_aux_reg(CTOP_AUX_THREAD_ID) == 0)
		nps_clkevent_timer_event_setup(nps_timer0_freq / HZ);

	return 0;
}

static int nps_clkevent_set_oneshot(struct clock_event_device *dev)
{
	nps_clkevent_rm_thread(true);
	nps_clkevent_timer_shutdown(dev);

	return 0;
}

static DEFINE_PER_CPU(struct clock_event_device, nps_clockevent_device) = {
	.name				=	"NPS Timer0",
	.features			=	CLOCK_EVT_FEAT_ONESHOT |
						CLOCK_EVT_FEAT_PERIODIC,
	.rating				=	300,
	.set_next_event			=	nps_clkevent_set_next_event,
	.set_state_periodic		=	nps_clkevent_set_periodic,
	.set_state_oneshot		=	nps_clkevent_set_oneshot,
	.set_state_oneshot_stopped	=	nps_clkevent_timer_shutdown,
	.set_state_shutdown		=	nps_clkevent_timer_shutdown,
	.tick_resume			=	nps_clkevent_timer_shutdown,
};

static irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
	/*
	 * Note that generic IRQ core could have passed @evt for @dev_id if
	 * irq_set_chip_and_handler() asked for handle_percpu_devid_irq()
	 */
	struct clock_event_device *evt = this_cpu_ptr(&nps_clockevent_device);
	int irq_reenable = clockevent_state_periodic(evt);

	nps_clkevent_rm_thread(!irq_reenable);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int nps_timer_cpu_notify(struct notifier_block *self,
				unsigned long action, void *hcpu)
{
	struct clock_event_device *evt = this_cpu_ptr(&nps_clockevent_device);

	evt->cpumask = cpumask_of(smp_processor_id());

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		clockevents_config_and_register(evt, nps_timer0_freq,
						0, ULONG_MAX);
		enable_percpu_irq(nps_timer0_irq, 0);
		break;
	case CPU_DYING:
		disable_percpu_irq(nps_timer0_irq);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block nps_timer_cpu_nb = {
	.notifier_call = nps_timer_cpu_notify,
};

static int __init nps_setup_clockevent(struct device_node *node)
{
	struct clock_event_device *evt = this_cpu_ptr(&nps_clockevent_device);
	struct clk *clk;
	int ret;

	nps_timer0_irq = irq_of_parse_and_map(node, 0);
	if (nps_timer0_irq <= 0) {
		pr_err("Can't parse IRQ");
		return -EINVAL;
	}

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("Can't get timer clock.\n");
		return PTR_ERR(clk);
	}

	register_cpu_notifier(&nps_timer_cpu_nb);

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("Couldn't enable parent clock\n");
		return ret;
	}

	nps_timer0_freq = clk_get_rate(clk);
	evt->irq = nps_timer0_irq;
	evt->cpumask = cpumask_of(smp_processor_id());
	clockevents_config_and_register(evt, nps_timer0_freq,
					0, NPS_TIMER_MAX);

	/* Needs apriori irq_set_percpu_devid() done in intc map function */
	ret = request_percpu_irq(nps_timer0_irq, timer_irq_handler,
				 "Timer0 (per-cpu-tick)", evt);
	if (ret) {
		pr_err("Couldn't request irq\n");
		clk_disable_unprepare(clk);
		return ret;
	}

	enable_percpu_irq(nps_timer0_irq, 0);
	return 0;
}

CLOCKSOURCE_OF_DECLARE(ezchip_nps400_clkevt, "ezchip,nps400-timer0",
		       nps_setup_clockevent);
#endif /* CONFIG_EZNPS_MTM_EXT */
