/*
 * Task isolation related global functions
 */
#ifndef _LINUX_ISOLATION_H
#define _LINUX_ISOLATION_H

#include <stdarg.h>
#include <linux/cpumask.h>
#include <linux/prctl.h>

struct task_struct;

#ifdef CONFIG_TASK_ISOLATION

/* cpus that are configured to support task isolation */
extern cpumask_var_t task_isolation_map;

extern int task_isolation_init(void);

static inline bool task_isolation_possible(int cpu)
{
	return task_isolation_map != NULL &&
		cpumask_test_cpu(cpu, task_isolation_map);
}

extern int task_isolation_set(unsigned int flags);

extern bool task_isolation_ready(void);
extern void task_isolation_prepare(void);

#define task_isolation_set_flags(p, flags)				\
	do {								\
		p->task_isolation_flags = flags;			\
		if (flags & PR_TASK_ISOLATION_ENABLE)			\
			set_tsk_thread_flag(p, TIF_TASK_ISOLATION);	\
		else							\
			clear_tsk_thread_flag(p, TIF_TASK_ISOLATION);	\
	} while (0)

extern int task_isolation_syscall(int nr);

/* Report on exceptions that don't cause a signal for the user process. */
extern void _task_isolation_quiet_exception(const char *fmt, ...);
#define task_isolation_quiet_exception(fmt, ...)			\
	do {								\
		if (current_thread_info()->flags & _TIF_TASK_ISOLATION) \
			_task_isolation_quiet_exception(fmt, ## __VA_ARGS__); \
	} while (0)

/* Debug interrupts that disturb task isolation. */
#define task_isolation_debug(cpu, fmt, ...)				\
	do {								\
		if (task_isolation_possible(cpu))			\
			_task_isolation_debug(cpu, fmt, ## __VA_ARGS__); \
	} while (0)
#define task_isolation_debug_cpumask(mask, fmt, ...)			\
	do {								\
		int cpu, thiscpu = get_cpu();				\
		for_each_cpu_and(cpu, mask, task_isolation_map)		\
		if (cpu != thiscpu)					\
			_task_isolation_debug(cpu, fmt, ## __VA_ARGS__); \
		put_cpu();						\
	} while (0)
extern void _task_isolation_debug(int cpu, const char *fmt, ...);
extern void task_isolation_debug_task(int cpu, struct task_struct *p,
				      const char *fmt, va_list args);
extern void task_isolation_irq(struct pt_regs *regs, const char *fmt, ...);
#else
static inline void task_isolation_init(void) { }
static inline bool task_isolation_possible(int cpu) { return false; }
static inline bool task_isolation_ready(void) { return true; }
static inline void task_isolation_prepare(void) { }
#define task_isolation_set_flags(p, flags) do {} while (0)
static inline int task_isolation_syscall(int nr) { return 0; }
static inline void task_isolation_quiet_exception(const char *fmt, ...) { }
#define task_isolation_debug(cpu, fmt, ...) do {} while (0)
#define task_isolation_debug_cpumask(mask, fmt, ...) do {} while (0)
static inline void task_isolation_irq(struct pt_regs *regs,
				      const char *fmt, ...) { }
#endif

#endif
