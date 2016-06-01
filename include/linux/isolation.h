/*
 * Task isolation related global functions
 */
#ifndef _LINUX_ISOLATION_H
#define _LINUX_ISOLATION_H

#include <linux/tick.h>
#include <linux/prctl.h>

#ifdef CONFIG_TASK_ISOLATION

/* cpus that are configured to support task isolation */
extern cpumask_var_t task_isolation_map;

static inline bool task_isolation_possible(int cpu)
{
	return tick_nohz_full_enabled() &&
		cpumask_test_cpu(cpu, task_isolation_map);
}

extern int task_isolation_set(unsigned int flags);

static inline bool task_isolation_enabled(void)
{
	bool temp;

	temp = task_isolation_possible(get_cpu()) &&
		(current->task_isolation_flags & PR_TASK_ISOLATION_ENABLE);
	put_cpu();

	return temp;
}

extern bool _task_isolation_ready(void);
extern void _task_isolation_enter(void);

static inline bool task_isolation_ready(void)
{
	return !task_isolation_enabled() || _task_isolation_ready();
}

static inline void task_isolation_enter(void)
{
	if (task_isolation_enabled())
		_task_isolation_enter();
}

extern bool task_isolation_syscall(int nr);
extern void task_isolation_exception(const char *fmt, ...);
extern void task_isolation_interrupt(struct task_struct *, const char *buf);
extern void task_isolation_debug(int cpu);
extern void task_isolation_debug_cpumask(const struct cpumask *);
extern void task_isolation_debug_task(int cpu, struct task_struct *p);

static inline bool task_isolation_strict(void)
{
	bool temp;

	temp = (task_isolation_possible(get_cpu()) &&
		(current->task_isolation_flags &
		 (PR_TASK_ISOLATION_ENABLE | PR_TASK_ISOLATION_STRICT)) ==
		(PR_TASK_ISOLATION_ENABLE | PR_TASK_ISOLATION_STRICT));
	put_cpu();

	return temp;
}

static inline bool task_isolation_check_syscall(int nr)
{
	return task_isolation_strict() && task_isolation_syscall(nr);
}

#define task_isolation_check_exception(fmt, ...)			\
	do {								\
		if (task_isolation_strict())				\
			task_isolation_exception(fmt, ## __VA_ARGS__);	\
	} while (0)

#else
static inline bool task_isolation_possible(int cpu) { return false; }
static inline bool task_isolation_enabled(void) { return false; }
static inline bool task_isolation_ready(void) { return true; }
static inline void task_isolation_enter(void) { }
static inline bool task_isolation_check_syscall(int nr) { return false; }
static inline void task_isolation_check_exception(const char *fmt, ...) { }
static inline void task_isolation_debug(int cpu) { }
#define task_isolation_debug_cpumask(mask) do {} while (0)
#endif

#endif
