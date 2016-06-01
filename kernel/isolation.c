/*
 *  linux/kernel/isolation.c
 *
 *  Implementation for task isolation.
 *
 *  Distributed under GPLv2.
 */

#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/isolation.h>
#include <linux/syscalls.h>
#include <linux/ratelimit.h>
#include <asm/unistd.h>
#include "time/tick-sched.h"

cpumask_var_t task_isolation_map;
static bool cpumask_allocated;

/*
 * Isolation requires both nohz and isolcpus support from the scheduler.
 * We provide a boot flag that enables both for now, and which we can
 * add other functionality to over time if needed.  Note that just
 * specifying "nohz_full=... isolcpus=..." does not enable task isolation.
 */
static int __init task_isolation_setup(char *str)
{
	cpumask_allocated = true;

	alloc_bootmem_cpumask_var(&task_isolation_map);
	if (cpulist_parse(str, task_isolation_map) < 0) {
		pr_warn("task_isolation: Incorrect cpumask '%s'\n", str);
		return 1;
	}

	alloc_bootmem_cpumask_var(&cpu_isolated_map);
	cpumask_copy(cpu_isolated_map, task_isolation_map);

	alloc_bootmem_cpumask_var(&tick_nohz_full_mask);
	cpumask_copy(tick_nohz_full_mask, task_isolation_map);
	tick_nohz_full_running = true;

	return 1;
}
__setup("task_isolation=", task_isolation_setup);

/* For offstack cpumask, ensure we allocate an empty cpumask early. */
static int __init task_isolation_cpumask_alloc(void)
{
	if (!cpumask_allocated)
		alloc_cpumask_var(&task_isolation_map, GFP_KERNEL);
	return 0;
}
early_initcall(task_isolation_cpumask_alloc)

/*
 * This routine controls whether we can enable task-isolation mode.
 * The task must be affinitized to a single task_isolation core or we will
 * return EINVAL.  Although the application could later re-affinitize
 * to a housekeeping core and lose task isolation semantics, this
 * initial test should catch 99% of bugs with task placement prior to
 * enabling task isolation.
 */
int task_isolation_set(unsigned int flags)
{
	if (cpumask_weight(tsk_cpus_allowed(current)) != 1 ||
	    !task_isolation_possible(smp_processor_id()))
		return -EINVAL;

	current->task_isolation_flags = flags;
	return 0;
}

/*
 * In task isolation mode we try to return to userspace only after
 * attempting to make sure we won't be interrupted again.  To handle
 * the periodic scheduler tick, we test to make sure that the tick is
 * stopped, and if it isn't yet, we request a reschedule so that if
 * another task needs to run to completion first, it can do so.
 * Similarly, if any other subsystems require quiescing, we will need
 * to do that before we return to userspace.
 */
bool _task_isolation_ready(void)
{
	WARN_ON_ONCE(!irqs_disabled());

	/* If we need to drain the LRU cache, we're not ready. */
	if (lru_add_drain_needed(smp_processor_id()))
		return false;

	/* If vmstats need updating, we're not ready. */
	if (!vmstat_idle())
		return false;

	/* Request rescheduling unless we are in full dynticks mode. */
	if (!tick_nohz_tick_stopped()) {
		set_tsk_need_resched(current);
		return false;
	}

	return true;
}

/*
 * Each time we try to prepare for return to userspace in a process
 * with task isolation enabled, we run this code to quiesce whatever
 * subsystems we can readily quiesce to avoid later interrupts.
 */
void _task_isolation_enter(void)
{
	WARN_ON_ONCE(irqs_disabled());

	/* Drain the pagevecs to avoid unnecessary IPI flushes later. */
	lru_add_drain();

	/* Quieten the vmstat worker so it won't interrupt us. */
	quiet_vmstat();
}

void task_isolation_interrupt(struct task_struct *task, const char *buf)
{
	siginfo_t info = {};
	int sig;

	pr_warn("%s/%d: task_isolation strict mode violated by %s\n",
		task->comm, task->pid, buf);

	/*
	 * Turn off task isolation mode entirely to avoid spamming
	 * the process with signals.  It can re-enable task isolation
	 * mode in the signal handler if it wants to.
	 */
	task->task_isolation_flags = 0;

	sig = PR_TASK_ISOLATION_GET_SIG(task->task_isolation_flags);
	if (sig == 0)
		sig = SIGKILL;
	info.si_signo = sig;
	send_sig_info(sig, &info, task);
}

/*
 * This routine is called from any userspace exception if the _STRICT
 * flag is set.
 */
void task_isolation_exception(const char *fmt, ...)
{
	va_list args;
	char buf[100];

	/* RCU should have been enabled prior to this point. */
	rcu_lockdep_assert(!rcu_is_watching(), "kernel entry without RCU");

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	task_isolation_interrupt(current, buf);
}

/*
 * This routine is called from syscall entry (with the syscall number
 * passed in) if the _STRICT flag is set.
 */
bool task_isolation_syscall(int syscall)
{
	/* Ignore prctl() syscalls or any task exit. */
	switch (syscall) {
	case __NR_prctl:
	case __NR_exit:
	case __NR_exit_group:
		return false;
	}

	task_isolation_exception("syscall %d", syscall);
	return true;
}

/* Enable debugging of any interrupts of task_isolation cores. */
static int task_isolation_debug_flag;
static int __init task_isolation_debug_func(char *str)
{
	task_isolation_debug_flag = true;
	return 1;
}
__setup("task_isolation_debug", task_isolation_debug_func);

void task_isolation_debug_task(int cpu, struct task_struct *p)
{
	static DEFINE_RATELIMIT_STATE(console_output, HZ, 1);
	bool force_debug = false;

	/*
	 * Our caller made sure the task was running on a task isolation
	 * core, but make sure the task has enabled isolation.
	 */
	if (!(p->task_isolation_flags & PR_TASK_ISOLATION_ENABLE))
		return;

	/*
	 * If the task was in strict mode, deliver a signal to it.
	 * We disable task isolation mode when we deliver a signal
	 * so we won't end up recursing back here again.
	 * If we are in an NMI, we don't try delivering the signal
	 * and instead just treat it as if "debug" mode was enabled,
	 * since that's pretty much all we can do.
	 */
	if (p->task_isolation_flags & PR_TASK_ISOLATION_STRICT) {
		if (in_nmi())
			force_debug = true;
		else
			task_isolation_interrupt(p, "interrupt");
	}

	/*
	 * If (for example) the timer interrupt starts ticking
	 * unexpectedly, we will get an unmanageable flow of output,
	 * so limit to one backtrace per second.
	 */
	if (force_debug ||
	    (task_isolation_debug_flag && __ratelimit(&console_output))) {
		pr_err("Interrupt detected for task_isolation cpu %d, %s/%d\n",
		       cpu, p->comm, p->pid);
		dump_stack();
	}
}

void task_isolation_debug_cpumask(const struct cpumask *mask)
{
	int cpu, thiscpu = smp_processor_id();

	/* No need to report on this cpu since we're already in the kernel. */
	for_each_cpu(cpu, mask)
		if (cpu != thiscpu)
			task_isolation_debug(cpu);
}
