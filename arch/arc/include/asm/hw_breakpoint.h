#ifndef _ARC_HW_BREAKPOINT_H
#define _ARC_HW_BREAKPOINT_H

#ifdef __KERNEL__

#ifdef CONFIG_HAVE_HW_BREAKPOINT

struct arch_hw_breakpoint_ctrl {
	union {
		struct {
#ifdef CONFIG_CPU_BIG_ENDIAN
			u32 len		: 8, /* logic len, for ptrace convers */
			__reserved	: 9,
			tidmatch	: 1,
			tid		: 4,
			quad		: 1,
			action		: 1,
			paired		: 1,
			mode		: 1,
			type		: 2,
			target		: 4;
#else
			u32 target	: 4,
			type		: 2,
			mode		: 1,
			paired		: 1,
			action		: 1,
			quad		: 1,
			tid		: 4,
			tidmatch	: 1,
			__reserved	: 9,
			len		: 8; /* logic len, for ptrace convers */
#endif
		};
		u32 value;
	};
};

struct arch_hw_breakpoint {
	u32		address;
	u32		trigger;
	unsigned int	type;
	struct	arch_hw_breakpoint_ctrl ctrl;
};

/* Breakpoint Type */
#define ARC_BREAKPOINT_DISABLED 0
#define ARC_BREAKPOINT_WRITE	1
#define ARC_BREAKPOINT_READ	2
#define ARC_BREAKPOINT_RW	3
#define ARC_BREAKPOINT_EXECUTE	0

/* Lengths */
#define ARC_BREAKPOINT_LEN_1	1
#define ARC_BREAKPOINT_LEN_2	2
#define ARC_BREAKPOINT_LEN_4	4
#define ARC_BREAKPOINT_LEN_8	8

/* Actions */
#define ARC_BREAKPOINT_ACTION_HALT	0
#define ARC_BREAKPOINT_ACTION_SOFT_EXP	1

/* Targets */
#define ARC_BREAKPOINT_TARGET_INST_ADDR		0
#define ARC_BREAKPOINT_TARGET_INST_DATA		1
#define ARC_BREAKPOINT_TARGET_LDST_ADDR		2
#define ARC_BREAKPOINT_TARGET_LDST_DATA		3
#define ARC_BREAKPOINT_TARGET_AUX_ADDR		4
#define ARC_BREAKPOINT_TARGET_AUX_DATA		5
#define ARC_BREAKPOINT_TARGET_EXT_PARAM0	6
#define ARC_BREAKPOINT_TARGET_EXT_PARAM1	7

/* Auxiliary debug registers. */
#define ARC_AP_BUILD		0x76
#define ARC_DEBUG		0x05

/* Base register numbers for the debug registers. */
#define ARC_BASE_AMV		0x220
#define ARC_BASE_AMM		0x221
#define ARC_BASE_AC		0x222

/* Maximum number of HW breakpoint registers */
#define ARC_MAX_HBP_SLOTS 8

struct notifier_block;
struct perf_event;
struct task_struct;

extern int arch_bp_generic_fields(struct arch_hw_breakpoint_ctrl ctrl,
		int *gen_len, int *gen_type);
extern int arch_check_bp_in_kernelspace(struct perf_event *bp);
extern int arch_validate_hwbkpt_settings(struct perf_event *bp);
extern int hw_breakpoint_exceptions_notify(struct notifier_block *unused,
					   unsigned long val, void *data);
extern void clear_ptrace_hw_breakpoint(struct task_struct *tsk);
int arch_install_hw_breakpoint(struct perf_event *bp);
void arch_uninstall_hw_breakpoint(struct perf_event *bp);
void hw_breakpoint_pmu_read(struct perf_event *bp);
int hw_breakpoint_slots(int type);

#else
static inline void clear_ptrace_hw_breakpoint(struct task_struct *tsk) {}

#endif	/* CONFIG_HAVE_HW_BREAKPOINT */
#endif	/* __KERNEL__ */
#endif	/* _ARC_HW_BREAKPOINT_H */
