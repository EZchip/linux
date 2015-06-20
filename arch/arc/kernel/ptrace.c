/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ptrace.h>
#include <linux/tracehook.h>
#include <linux/regset.h>
#include <linux/unistd.h>
#include <linux/elf.h>
#include <linux/hw_breakpoint.h>
#include <linux/user.h>
#include <linux/context_tracking.h>
#include <linux/thread_info.h>

static struct callee_regs *task_callee_regs(struct task_struct *tsk)
{
	struct callee_regs *tmp = (struct callee_regs *)tsk->thread.callee_reg;
	return tmp;
}

static int genregs_get(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       void *kbuf, void __user *ubuf)
{
	const struct pt_regs *ptregs = task_pt_regs(target);
	const struct callee_regs *cregs = task_callee_regs(target);
	int ret = 0;
	unsigned int stop_pc_val;

#define REG_O_CHUNK(START, END, PTR)	\
	if (!ret)	\
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, PTR, \
			offsetof(struct user_regs_struct, START), \
			offsetof(struct user_regs_struct, END));

#define REG_O_ONE(LOC, PTR)	\
	if (!ret)		\
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, PTR, \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

#define REG_O_ZERO(LOC)		\
	if (!ret)		\
		ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf, \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

	REG_O_ZERO(pad);
	REG_O_ONE(scratch.bta, &ptregs->bta);
	REG_O_ONE(scratch.lp_start, &ptregs->lp_start);
	REG_O_ONE(scratch.lp_end, &ptregs->lp_end);
	REG_O_ONE(scratch.lp_count, &ptregs->lp_count);
	REG_O_ONE(scratch.status32, &ptregs->status32);
	REG_O_ONE(scratch.ret, &ptregs->ret);
	REG_O_ONE(scratch.blink, &ptregs->blink);
	REG_O_ONE(scratch.fp, &ptregs->fp);
	REG_O_ONE(scratch.gp, &ptregs->r26);
	REG_O_ONE(scratch.r12, &ptregs->r12);
	REG_O_ONE(scratch.r11, &ptregs->r11);
	REG_O_ONE(scratch.r10, &ptregs->r10);
	REG_O_ONE(scratch.r9, &ptregs->r9);
	REG_O_ONE(scratch.r8, &ptregs->r8);
	REG_O_ONE(scratch.r7, &ptregs->r7);
	REG_O_ONE(scratch.r6, &ptregs->r6);
	REG_O_ONE(scratch.r5, &ptregs->r5);
	REG_O_ONE(scratch.r4, &ptregs->r4);
	REG_O_ONE(scratch.r3, &ptregs->r3);
	REG_O_ONE(scratch.r2, &ptregs->r2);
	REG_O_ONE(scratch.r1, &ptregs->r1);
	REG_O_ONE(scratch.r0, &ptregs->r0);
	REG_O_ONE(scratch.sp, &ptregs->sp);

	REG_O_ZERO(pad2);

	REG_O_ONE(callee.r25, &cregs->r25);
	REG_O_ONE(callee.r24, &cregs->r24);
	REG_O_ONE(callee.r23, &cregs->r23);
	REG_O_ONE(callee.r22, &cregs->r22);
	REG_O_ONE(callee.r21, &cregs->r21);
	REG_O_ONE(callee.r20, &cregs->r20);
	REG_O_ONE(callee.r19, &cregs->r19);
	REG_O_ONE(callee.r18, &cregs->r18);
	REG_O_ONE(callee.r17, &cregs->r17);
	REG_O_ONE(callee.r16, &cregs->r16);
	REG_O_ONE(callee.r15, &cregs->r15);
	REG_O_ONE(callee.r14, &cregs->r14);
	REG_O_ONE(callee.r13, &cregs->r13);

	REG_O_ONE(efa, &target->thread.fault_address);

	if (!ret) {
		if (in_brkpt_trap(ptregs)) {
			stop_pc_val = target->thread.fault_address;
			pr_debug("\t\tstop_pc (brk-pt)\n");
		} else {
			stop_pc_val = ptregs->ret;
			pr_debug("\t\tstop_pc (others)\n");
		}

		REG_O_ONE(stop_pc, &stop_pc_val);
	}

	return ret;
}

static int genregs_set(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       const void *kbuf, const void __user *ubuf)
{
	const struct pt_regs *ptregs = task_pt_regs(target);
	const struct callee_regs *cregs = task_callee_regs(target);
	int ret = 0;

#define REG_IN_CHUNK(FIRST, NEXT, PTR)	\
	if (!ret)			\
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, \
			(void *)(PTR), \
			offsetof(struct user_regs_struct, FIRST), \
			offsetof(struct user_regs_struct, NEXT));

#define REG_IN_ONE(LOC, PTR)		\
	if (!ret)			\
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, \
			(void *)(PTR), \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

#define REG_IGNORE_ONE(LOC)		\
	if (!ret)			\
		ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf, \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

	REG_IGNORE_ONE(pad);

	REG_IN_ONE(scratch.bta, &ptregs->bta);
	REG_IN_ONE(scratch.lp_start, &ptregs->lp_start);
	REG_IN_ONE(scratch.lp_end, &ptregs->lp_end);
	REG_IN_ONE(scratch.lp_count, &ptregs->lp_count);

	REG_IGNORE_ONE(scratch.status32);

	REG_IN_ONE(scratch.ret, &ptregs->ret);
	REG_IN_ONE(scratch.blink, &ptregs->blink);
	REG_IN_ONE(scratch.fp, &ptregs->fp);
	REG_IN_ONE(scratch.gp, &ptregs->r26);
	REG_IN_ONE(scratch.r12, &ptregs->r12);
	REG_IN_ONE(scratch.r11, &ptregs->r11);
	REG_IN_ONE(scratch.r10, &ptregs->r10);
	REG_IN_ONE(scratch.r9, &ptregs->r9);
	REG_IN_ONE(scratch.r8, &ptregs->r8);
	REG_IN_ONE(scratch.r7, &ptregs->r7);
	REG_IN_ONE(scratch.r6, &ptregs->r6);
	REG_IN_ONE(scratch.r5, &ptregs->r5);
	REG_IN_ONE(scratch.r4, &ptregs->r4);
	REG_IN_ONE(scratch.r3, &ptregs->r3);
	REG_IN_ONE(scratch.r2, &ptregs->r2);
	REG_IN_ONE(scratch.r1, &ptregs->r1);
	REG_IN_ONE(scratch.r0, &ptregs->r0);
	REG_IN_ONE(scratch.sp, &ptregs->sp);

	REG_IGNORE_ONE(pad2);

	REG_IN_ONE(callee.r25, &cregs->r25);
	REG_IN_ONE(callee.r24, &cregs->r24);
	REG_IN_ONE(callee.r23, &cregs->r23);
	REG_IN_ONE(callee.r22, &cregs->r22);
	REG_IN_ONE(callee.r21, &cregs->r21);
	REG_IN_ONE(callee.r20, &cregs->r20);
	REG_IN_ONE(callee.r19, &cregs->r19);
	REG_IN_ONE(callee.r18, &cregs->r18);
	REG_IN_ONE(callee.r17, &cregs->r17);
	REG_IN_ONE(callee.r16, &cregs->r16);
	REG_IN_ONE(callee.r15, &cregs->r15);
	REG_IN_ONE(callee.r14, &cregs->r14);
	REG_IN_ONE(callee.r13, &cregs->r13);

	REG_IGNORE_ONE(efa);			/* efa update invalid */
	REG_IGNORE_ONE(stop_pc);		/* PC updated via @ret */

	return ret;
}

enum arc_getset {
	REGSET_GENERAL,
};

#ifdef CONFIG_HAVE_HW_BREAKPOINT
enum arc_hbp_register {
	hw_breakpoint_value	= 0,	/* HW breakpoint value register */
	hw_breakpoint_control	= 1,	/* HW breakpoint control register */
	hw_breakpoint_mask	= 2,	/* HW breakpoint mask register */
};
#endif

static const struct user_regset arc_regsets[] = {
	[REGSET_GENERAL] = {
	       .core_note_type = NT_PRSTATUS,
	       .n = ELF_NGREG,
	       .size = sizeof(unsigned long),
	       .align = sizeof(unsigned long),
	       .get = genregs_get,
	       .set = genregs_set,
	}
};

static const struct user_regset_view user_arc_view = {
	.name		= UTS_MACHINE,
	.e_machine	= EM_ARC_INUSE,
	.regsets	= arc_regsets,
	.n		= ARRAY_SIZE(arc_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_arc_view;
}

void ptrace_disable(struct task_struct *child)
{
}

#ifdef CONFIG_HAVE_HW_BREAKPOINT
/*
 * Handle hitting a HW-breakpoint.
 */
static void ptrace_hbptriggered(struct perf_event *bp,
				     struct perf_sample_data *data,
				     struct pt_regs *regs)
{
	struct arch_hw_breakpoint *bkpt = counter_arch_bp(bp);
	struct task_struct *tsk = current;
	int i;
	long num;
	siginfo_t info;

	for (i = 0; i < ARC_MAX_HBP_SLOTS; ++i)
		if (current->thread.hbp[i] == bp)
			break;

	if (i == ARC_MAX_HBP_SLOTS)
		num = 0;
	else {
		num = i + 1;
		/* mark as breakpoint if target instruction address */
		if (bkpt->ctrl.target == 0)
			num = 0 - num;
	}

	info.si_signo	= SIGTRAP;
	info.si_errno	= (int)num;
	info.si_code	= TRAP_HWBKPT;
	info.si_addr	= (void __user *)(bkpt->trigger);

	tsk->thread.fault_address = (__force unsigned int)info.si_addr;

	force_sig_info(SIGTRAP, &info, current);
}

static struct perf_event *ptrace_hbp_create(struct task_struct *tsk)
{
	struct perf_event_attr attr;

	ptrace_breakpoint_init(&attr);

	/* Initialise fields to sane defaults. */
	attr.bp_addr	= 0;
	attr.bp_len	= HW_BREAKPOINT_LEN_4;
	attr.bp_type	= HW_BREAKPOINT_RW;
	attr.disabled	= 1;

	return register_user_hw_breakpoint(&attr, ptrace_hbptriggered, NULL,
					   tsk);
}

static int ptrace_set_hbp(struct task_struct *tsk, int nr,
		enum arc_hbp_register regtype, unsigned long data)
{
	struct thread_struct *t = &tsk->thread;
	struct perf_event *bp = t->hbp[nr];
	int gen_type, gen_len, err = 0;
	struct perf_event_attr attr;
	struct arch_hw_breakpoint_ctrl ctrl;

	if (!(nr < ARC_MAX_HBP_SLOTS)) {
		err = -EIO;
		goto out;
	}

	if (!bp) {
		bp = ptrace_hbp_create(tsk);
		if (IS_ERR(bp)) {
			err = PTR_ERR(bp);
			goto out;
		} else
			t->hbp[nr] = bp;
	}

	attr = bp->attr;

	switch (regtype) {
	case hw_breakpoint_value:
		attr.bp_addr = data;
		break;
	case hw_breakpoint_control:
		ctrl.value = data;
		err = arch_bp_generic_fields(ctrl, &gen_len, &gen_type);
		if (err)
			goto out;
		attr.bp_type	= gen_type;
		attr.bp_len	= gen_len;
		attr.disabled	= gen_type == 0 ? 1 : 0;
		break;
	default:
		err = -EIO;
		goto out;
	}
	err = modify_user_hw_breakpoint(bp, &attr);

out:
	return err;
}

/*
 * Unregister breakpoints from this task and reset the pointers in
 * the thread_struct.
 */
void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	int i;
	struct thread_struct *t = &tsk->thread;

	for (i = 0; i < ARC_MAX_HBP_SLOTS; i++) {
		if (t->hbp[i]) {
			unregister_hw_breakpoint(t->hbp[i]);
			t->hbp[i] = NULL;
		}
	}
}

/*
 * Set ptrace breakpoint pointers to zero for this task.
 * This is required in order to prevent child processes from unregistering
 * breakpoints held by their parent.
 */
void clear_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	memset(tsk->thread.hbp, 0, sizeof(tsk->thread.hbp));
}

#endif /* CONFIG_HAVE_HW_BREAKPOINT */


long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret = -EIO;

	pr_debug("REQ=%ld: ADDR =0x%lx, DATA=0x%lx)\n", request, addr, data);

	switch (request) {
	case PTRACE_GET_THREAD_AREA:
		ret = put_user(task_thread_info(child)->thr_ptr,
			       (unsigned long __user *)data);
		break;
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	/* write the word at location addr in the USER area */
	case PTRACE_POKEUSR:
		ret = -EIO;
		if ((addr & (sizeof(data) - 1)) || addr >= sizeof(struct user))
			break;

		if (addr >= offsetof(struct user, u_dr_value[0]) &&
			addr <= offsetof(struct user, u_dr_value[7])) {
			addr -= offsetof(struct user, u_dr_value[0]);
			ret = ptrace_set_hbp(child, addr / sizeof(data),
					hw_breakpoint_value, data);
		} else if (addr >= offsetof(struct user, u_dr_control[0]) &&
			addr <= offsetof(struct user, u_dr_control[7])) {
			addr -= offsetof(struct user, u_dr_control[0]);
			ret = ptrace_set_hbp(child, addr / sizeof(data),
					hw_breakpoint_control, data);
		}
		break;
#endif /* CONFIG_HAVE_HW_BREAKPOINT */
	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

asmlinkage int syscall_trace_entry(struct pt_regs *regs)
{
	user_exit();

	if ((test_thread_flag(TIF_SYSCALL_TRACE)) &&
	    tracehook_report_syscall_entry(regs))
		return ULONG_MAX;

	return regs->r8;
}

asmlinkage void syscall_trace_exit(struct pt_regs *regs)
{
	/*
	 * We may come here right after calling schedule_user()
	 * or do_notify_resume(), in which case we can be in RCU
	 * user mode.
	 */
	user_exit();

	if (test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, 0);

	user_enter();
}
