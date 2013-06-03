/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * RajeshwarR: Dec 11, 2007
 *   -- Added support for Inter Processor Interrupts
 *
 * Vineetg: Nov 1st, 2007
 *    -- Initial Write (Borrowed heavily from ARM)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/profile.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/spinlock_types.h>
#include <asm/processor.h>
#include <asm/setup.h>

arch_spinlock_t smp_atomic_ops_lock = __ARCH_SPIN_LOCK_UNLOCKED;
arch_spinlock_t smp_bitops_lock = __ARCH_SPIN_LOCK_UNLOCKED;

/* XXX: per cpu ? Only needed once in early seconday boot */
struct task_struct *secondary_idle_tsk;

/* Called from start_kernel */
void __init smp_prepare_boot_cpu(void)
{
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i;

	for (i = 0; i < NR_CPUS; i++)
		cpu_set(i, cpu_possible_map);
}

/* called from init ( ) =>  process 1 */
void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		cpu_set(i, cpu_present_map);
}

void __init smp_cpus_done(unsigned int max_cpus)
{

}

/*
 * The very first "C" code executed by secondary
 * Called from asm stub in head.S
 * "current"/R25 already setup by low level boot code
 */
void __cpuinit start_kernel_secondary(void)
{
	struct mm_struct *mm = &init_mm;
	unsigned int cpu = smp_processor_id();

	/* MMU, Caches, Vector Table, Interrupts etc */
	setup_processor();

	atomic_inc(&mm->mm_users);
	atomic_inc(&mm->mm_count);
	current->active_mm = mm;

	set_cpu_online(cpu, true);

	pr_info("## CPU%u LIVE ##: Executing Code...\n", cpu);

	arc_platform_smp_init_cpu();

	/*
	 * XXX: Note that TIMER1 based cpu-local counters will not be
	 * synchronized given the time lag between time_init (boot-cpu only)
	 * vs. the time we reach here. However assuming that any SMP Linux
	 * deployments will use a recent enough ARC700 with a perfectly
	 * synchronized 64-bit CPU cycle counter backing the RTSC insn.
	 */
	arc_clock_counter_setup();

	arc_clockevent_init();

	local_irq_enable();
	preempt_disable();
	cpu_idle();
}

/*
 * Called from kernel_init( ) -> smp_init( ) - for each CPU
 *
 * At this point, Secondary Processor  is "HALT"ed:
 *  -It booted, but was halted in head.S
 *  -It was configured to halt-on-reset
 *  So need to wake it up.
 *
 * Essential requirements being where to run from (PC) and stack (SP)
*/
int __cpuinit __cpu_up(unsigned int cpu)
{
	struct task_struct *idle;
	unsigned long wait_till;

	idle = fork_idle(cpu);
	if (IS_ERR(idle)) {
		pr_err("CPU%u: fork() failed\n", cpu);
		return PTR_ERR(idle);
	}

	secondary_idle_tsk = idle;

	pr_info("Idle Task [%d] %p", cpu, idle);
	pr_info("Trying to bring up CPU%u ...\n", cpu);

	arc_platform_smp_wakeup_cpu(cpu,
				(unsigned long)first_lines_of_secondary);

	/* wait for 1 sec after kicking the secondary */
	wait_till = jiffies + HZ;
	while (time_before(jiffies, wait_till)) {
		if (cpu_online(cpu))
			break;
	}

	if (!cpu_online(cpu)) {
		pr_info("Timeout: CPU%u FAILED to comeup !!!\n", cpu);
		return -1;
	}

	return 0;
}

/*
 * not supported here
 */
int __init setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

/*****************************************************************************/
/*              Inter Processor Interrupt Handling                           */
/*****************************************************************************/

/*
 * structures for inter-processor calls
 * A Collection of single bit ipi messages
 *
 */

/*
 * TODO_rajesh investigate tlb message types.
 * IPI Timer not needed because each ARC has an individual Interrupting Timer
 */
enum ipi_msg_type {
	IPI_NOP = 0,
	IPI_RESCHEDULE = 1,
	IPI_CALL_FUNC,
	IPI_CALL_FUNC_SINGLE,
	IPI_CPU_STOP,
	IPI_IENABLE,
};

struct ipi_data {
	unsigned long bits;
};

static DEFINE_PER_CPU(struct ipi_data, ipi_data);

static DEFINE_SPINLOCK(ipi_ienable_lock);
static unsigned long ipi_ienable_reset;
static unsigned long ipi_ienable_set;

static void ipi_send_msg(const struct cpumask *callmap, enum ipi_msg_type msg)
{
	unsigned long flags;
	unsigned int cpu;

	local_irq_save(flags);

	for_each_cpu(cpu, callmap) {
		struct ipi_data *ipi = &per_cpu(ipi_data, cpu);
		set_bit(msg, &ipi->bits);
	}

	/* Call the platform specific cross-CPU call function  */
	arc_platform_ipi_send(callmap);

	local_irq_restore(flags);
}

static void ipi_ienable (void)
{
	unsigned int ienb;
	unsigned long flags;

	ienb = read_aux_reg(AUX_IENABLE);
	spin_lock_irqsave(&ipi_ienable_lock, flags);
	ienb |= ipi_ienable_set;
	ienb &= ~ipi_ienable_reset;
	spin_unlock_irqrestore(&ipi_ienable_lock, flags);
	write_aux_reg(AUX_IENABLE, ienb);
}

static void smp_send_ipi_ienable(void)

{
	int cpu;

	for_each_online_cpu(cpu)
	{
		if (cpu == smp_processor_id())
			ipi_ienable();
		else
			ipi_send_msg(cpumask_of(cpu), IPI_IENABLE);
	}
}

void smp_send_ienable(unsigned int irq, bool do_set)
{
	unsigned long flags;

	spin_lock_irqsave(&ipi_ienable_lock, flags);
	if (do_set)
	{
		set_bit(irq, &ipi_ienable_set);
		clear_bit(irq, &ipi_ienable_reset);
	}
	else
	{
		set_bit(irq, &ipi_ienable_reset);
		clear_bit(irq, &ipi_ienable_set);
	}
	spin_unlock_irqrestore(&ipi_ienable_lock, flags);

	smp_send_ipi_ienable();
}

void smp_send_reschedule(int cpu)
{
	ipi_send_msg(cpumask_of(cpu), IPI_RESCHEDULE);
}

void smp_send_stop(void)
{
	struct cpumask targets;
	cpumask_copy(&targets, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &targets);
	ipi_send_msg(&targets, IPI_CPU_STOP);
}

void arch_send_call_function_single_ipi(int cpu)
{
	ipi_send_msg(cpumask_of(cpu), IPI_CALL_FUNC_SINGLE);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	ipi_send_msg(mask, IPI_CALL_FUNC);
}

/*
 * ipi_cpu_stop - handle IPI from smp_send_stop()
 */
static void ipi_cpu_stop(unsigned int cpu)
{
	__asm__("flag 1");
}

static inline void __do_IPI(unsigned long *ops, struct ipi_data *ipi, int cpu)
{
	unsigned long msg = 0;

	do {
		msg = find_next_bit(ops, BITS_PER_LONG, msg+1);

		switch (msg) {
		case IPI_RESCHEDULE:
			scheduler_ipi();
			break;

		case IPI_CALL_FUNC:
			generic_smp_call_function_interrupt();
			break;

		case IPI_CALL_FUNC_SINGLE:
			generic_smp_call_function_single_interrupt();
			break;

		case IPI_CPU_STOP:
			ipi_cpu_stop(cpu);
			break;

		case IPI_IENABLE:
			ipi_ienable();
			break;
		}
	} while (msg < BITS_PER_LONG);

}

/*
 * arch-common ISR to handle for inter-processor interrupts
 * Has hooks for platform specific IPI
 */
irqreturn_t do_IPI(int irq, void *dev_id)
{
	int cpu = smp_processor_id();
	struct ipi_data *ipi = &per_cpu(ipi_data, cpu);
	unsigned long ops;

	arc_platform_ipi_clear(cpu, irq);

	/*
	 * XXX: is this loop really needed
	 * And do we need to move ipi_clean inside
	 */
	while ((ops = xchg(&ipi->bits, 0)) != 0)
		__do_IPI(&ops, ipi, cpu);

	return IRQ_HANDLED;
}

/*
 * API called by platform code to hookup arch-common ISR to their IPI IRQ
 */
static DEFINE_PER_CPU(int, ipi_dev);
int smp_ipi_irq_setup(int cpu, int irq)
{
	int *dev_id = &per_cpu(ipi_dev, smp_processor_id());
	return request_percpu_irq(irq, do_IPI, "IPI Interrupt", dev_id);
}

struct cpu cpu_topology[NR_CPUS];

static int __init topology_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
	    register_cpu(&cpu_topology[cpu], cpu);

	return 0;
}

subsys_initcall(topology_init);
