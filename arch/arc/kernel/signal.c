/*
 * Signal Handling for ARC
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: Jan 2010 (Restarting of timer related syscalls)
 *
 * vineetg: Nov 2009 (Everything needed for TIF_RESTORE_SIGMASK)
 *  -do_signal() supports TIF_RESTORE_SIGMASK
 *  -do_signal() no loner needs oldset, required by OLD sys_sigsuspend
 *  -sys_rt_sigsuspend() now comes from generic code, so discard arch implemen
 *  -sys_sigsuspend() no longer needs to fudge ptregs, hence that arg removed
 *  -sys_sigsuspend() no longer loops for do_signal(), sets TIF_xxx and leaves
 *   the job to do_signal()
 *
 * vineetg: July 2009
 *  -Modified Code to support the uClibc provided userland sigreturn stub
 *   to avoid kernel synthesing it on user stack at runtime, costing TLB
 *   probes and Cache line flushes.
 *  -In case uClibc doesnot provide the sigret stub, rewrote the PTE/TLB
 *   permission chg code to not duplicate the code in case of kernel stub
 *   straddling 2 pages
 *
 * vineetg: July 2009
 *  -In stash_usr_regs( ) and restore_usr_regs( ), save/restore of user regs
 *   in done in block copy rather than one word at a time.
 *   This saves around 2K of code and 500 lines of asm in just 2 functions,
 *   and LMB lat_sig "catch" numbers are lot better
 *
 * rajeshwarr: Feb 2009
 *  - Support for Realtime Signals
 *
 * vineetg: Feb 2009
 *  -small goofup in calculating if Frame straddles 2 pages
 *    now SUPER efficient
 *
 * vineetg: Aug 11th 2008: Bug #94183
 *  -ViXS were still seeing crashes when using insmod to load drivers.
 *   It turned out that the code to change Execute permssions for TLB entries
 *   of user was not guarded for interrupts (mod_tlb_permission)
 *   This was cauing TLB entries to be overwritten on unrelated indexes
 *
 * Vineetg: July 15th 2008: Bug #94183
 *  -Exception happens in Delay slot of a JMP, and before user space resumes,
 *   Signal is delivered (Ctrl + C) = >SIGINT.
 *   setup_frame( ) sets up PC,SP,BLINK to enable user space signal handler
 *   to run, but doesn't clear the Delay slot bit from status32. As a result,
 *   on resuming user mode, signal handler branches off to BTA of orig JMP
 *  -FIX: clear the DE bit from status32 in setup_frame( )
 *
 * Rahul Trivedi, Kanika Nema: Codito Technologies 2004
 */

#include <linux/signal.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/personality.h>
#include <linux/freezer.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <asm/ucontext.h>
#include <asm/tlb.h>
#include <asm/cacheflush.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

static noinline void set_frame_exec(unsigned long vaddr, unsigned int exec_on);

asmlinkage int sys_sigaltstack(const stack_t *uss, stack_t *uoss)
{
	struct pt_regs *regs = task_pt_regs(current);
	return do_sigaltstack(uss, uoss, regs->sp);
}

/*
 * Do a signal return; undo the signal stack.  These are aligned to 64-bit.
 */
struct sigframe {
	struct ucontext uc;
#define MAGIC_USERLAND_STUB     0x11097600
#define MAGIC_KERNEL_SYNTH_STUB 0x07300400
#define MAGIC_SIGALTSTK		0x00000001
	unsigned int sigret_magic;
	unsigned long retcode[5];
};

struct rt_sigframe {
	struct siginfo info;
	struct sigframe frame;
};

static int
stash_usr_regs(struct sigframe __user *sf, struct pt_regs *regs,
	       sigset_t *set)
{
	int err;
	err = __copy_to_user(&(sf->uc.uc_mcontext.regs), regs, sizeof(*regs));
	err |= __copy_to_user(&sf->uc.uc_sigmask, set, sizeof(sigset_t));

	return err;
}

static int restore_usr_regs(struct pt_regs *regs, struct sigframe __user *sf)
{
	sigset_t set;
	int err;

	err = __copy_from_user(&set, &sf->uc.uc_sigmask, sizeof(set));
	if (err == 0) {
		sigdelsetmask(&set, ~_BLOCKABLE);
		spin_lock_irq(&current->sighand->siglock);
		current->blocked = set;
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}

	err |= __copy_from_user(regs, &(sf->uc.uc_mcontext.regs),
				sizeof(*regs));

	return err;
}

static inline int is_sigret_synth(unsigned int magic)
{
	if ((MAGIC_KERNEL_SYNTH_STUB & magic) == MAGIC_KERNEL_SYNTH_STUB)
		return 1;
	else
		return 0;
}

static inline int is_sigret_userspace(unsigned int magic)
{
	if ((MAGIC_USERLAND_STUB & magic) == MAGIC_USERLAND_STUB)
		return 1;
	else
		return 0;
}

static inline int is_do_ss_needed(unsigned int magic)
{
	if ((MAGIC_SIGALTSTK & magic) == MAGIC_SIGALTSTK)
		return 1;
	else
		return 0;
}

SYSCALL_DEFINE0(rt_sigreturn)
{
	struct rt_sigframe __user *rtf;
	struct sigframe __user *sf;
	unsigned int magic;
	int err;
	struct pt_regs *regs = task_pt_regs(current);

	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	/* Since we stacked the signal on a word boundary,
	 * then 'sp' should be word aligned here.  If it's
	 * not, then the user is trying to mess with us.
	 */
	if (regs->sp & 3)
		goto badframe;

	rtf = ((struct rt_sigframe __user *)regs->sp);

	if (!access_ok(VERIFY_READ, rtf, sizeof(*rtf)))
		goto badframe;

	sf = &rtf->frame;
	err = restore_usr_regs(regs, sf);
	err |= __get_user(magic, &sf->sigret_magic);
	if (err)
		goto badframe;

	/*
	 * If C-lib provided userland sigret stub, no need to do anything.
	 * If kernel sythesized sigret stub, need to undo PTE/TLB changes
	 * for making stack executable.
	 */
	if (unlikely(is_sigret_synth(magic))) {
		set_frame_exec((unsigned long)&sf->retcode, 0);
	} else if (unlikely(!is_sigret_userspace(magic))) {
		/* user corrupted the signal stack */
		pr_notice("sys_rt_sigreturn: sig stack corrupted");
		goto badframe;
	}

	if (unlikely(is_do_ss_needed(magic)))
		if (do_sigaltstack(&sf->uc.uc_stack, NULL, regs->sp) == -EFAULT)
			goto badframe;

	take_snap(SNAP_SIGRETURN, 0, 0);

	return regs->r0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

/*
 * Determine which stack to use..
 */
static inline void *get_sigframe(struct k_sigaction *ka, struct pt_regs *regs,
				 unsigned long framesize)
{
	unsigned long sp = regs->sp;
	void __user *frame;

	/* This is the X/Open sanctioned signal stack switching */
	if ((ka->sa.sa_flags & SA_ONSTACK) && !sas_ss_flags(sp))
		sp = current->sas_ss_sp + current->sas_ss_size;

	/* No matter what happens, 'sp' must be word
	 * aligned otherwise nasty things could happen
	 */

	/* ATPCS B01 mandates 8-byte alignment */
	frame = (void __user *)((sp - framesize) & ~7);

	/* Check that we can actually write to the signal frame */
	if (!access_ok(VERIFY_WRITE, frame, framesize))
		frame = NULL;

	return frame;
}

static noinline int
create_sigret_stub(struct k_sigaction *ka, unsigned long *retcode)
{
	unsigned int code;
	int err;

	code = 0x12c2208a;	/* code for mov r8, __NR_rt_sigreturn */
	err = __put_user(code, retcode);
	code = 0x003f226f;	/* code for trap0 */
	err |= __put_user(code, retcode + 1);
	code = 0x7000264a;	/* code for nop */
	err |= __put_user(code, retcode + 2);
	code = 0x7000264a;	/* code for nop */
	err |= __put_user(code, retcode + 3);

	if (err)
		return err;

	/* Temp wiggle stack page to be exec, to allow synth sigreturn to run */
	set_frame_exec((unsigned long)retcode, 1);
	return 0;
}

/* Set up to return from userspace signal handler back into kernel */
static int
setup_ret_from_usr_sighdlr(struct k_sigaction *ka, struct pt_regs *regs,
			   struct sigframe __user *frame, unsigned int magic)
{
	unsigned long *retcode;
	int err = 0;

	/* If provided, use a stub already in userspace */
	if (likely(ka->sa.sa_flags & SA_RESTORER)) {
		magic |= MAGIC_USERLAND_STUB;
		retcode = (unsigned long *)ka->sa.sa_restorer;
	} else {
		magic |= MAGIC_KERNEL_SYNTH_STUB;
		/*
		 * Note that with uClibc providing userland sigreturn stub,
		 * this code is more of a legacy and will not be executed
		 * The really bad part was flushing the TLB and caches which
		 * we no longer have to do
		 */
		retcode = frame->retcode;
		err = create_sigret_stub(ka, retcode);
	}

	err |= __put_user(magic, &frame->sigret_magic);
	if (err)
		return err;

	/* Upon return of handler, enable sigreturn sys call (synth or real) */
	regs->blink = (unsigned long)retcode;

	take_snap(SNAP_BEFORE_SIG, 0, 0);

	return 0;
}

static int
setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
	       sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *rtf;
	struct sigframe __user *sf;
	unsigned int magic = 0;
	stack_t stk;
	int err = 0;

	rtf = get_sigframe(ka, regs, sizeof(struct rt_sigframe));
	if (!rtf)
		return 1;

	sf = &rtf->frame;

	/*
	 * SA_SIGINFO requires 3 args to signal handler:
	 *  #1: sig-no (common to any handler)
	 *  #2: struct siginfo
	 *  #3: struct ucontext (completely populated)
	 */
	if (unlikely(ka->sa.sa_flags & SA_SIGINFO)) {
		err |= copy_siginfo_to_user(&rtf->info, info);
		err |= __put_user(0, &sf->uc.uc_flags);
		err |= __put_user(NULL, &sf->uc.uc_link);
		stk.ss_sp = (void __user *)current->sas_ss_sp;
		stk.ss_flags = sas_ss_flags(regs->sp);
		stk.ss_size = current->sas_ss_size;
		err |= __copy_to_user(&sf->uc.uc_stack, &stk, sizeof(stk));

		/* setup args 2 and 3 fo ruse rmode handler */
		regs->r1 = (unsigned long)&rtf->info;
		regs->r2 = (unsigned long)&sf->uc;

		/*
		 * small optim to avoid unconditonally calling do_sigaltstack
		 * in sigreturn path, now that we only have rt_sigreturn
		 */
		magic = MAGIC_SIGALTSTK;
	}

	/*
	 * w/o SA_SIGINFO, struct ucontext is partially populated (only
	 * uc_mcontext/uc_sigmask) for kernel's normal user state preservation
	 * during signal handler execution. This works for SA_SIGINFO as well
	 * although the semantics are now overloaded (the same reg state can be
	 * inspected by userland: but are they allowed to fiddle with it ?
	 */
	err |= stash_usr_regs(sf, regs, set);
	err |= setup_ret_from_usr_sighdlr(ka, regs, sf, magic);
	if (err)
		return err;

	/* #1 arg to the user Signal handler */
	regs->r0 = sig;

	/* setup PC of user space signal handler */
	regs->ret = (unsigned long)ka->sa.sa_handler;

	/* User Stack for signal handler will be above the frame just carved */
	regs->sp = (unsigned long)rtf;

	/*
	 * Bug 94183, Clear the DE bit, so that when signal handler
	 * starts to run, it doesn't use BTA
	 */
	regs->status32 &= ~STATUS_DE_MASK;
	regs->status32 |= STATUS_L_MASK;

	return err;
}

/*
 * OK, we're invoking a handler
 */
static int
handle_signal(unsigned long sig, struct k_sigaction *ka, siginfo_t *info,
	      sigset_t *oldset, struct pt_regs *regs, int in_syscall)
{
	struct thread_info *thread = current_thread_info();
	unsigned long usig = sig;
	int ret;

	/* Syscall restarting based on the ret code and other criteria */
	if (in_syscall) {
		switch (regs->r0) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			/*
			 * ERESTARTNOHAND means that the syscall should
			 * only be restarted if there was no handler for
			 * the signal, and since we only get here if there
			 * is a handler, we don't restart
			 */
			regs->r0 = -EINTR;   /* ERESTART_xxx is internal */
			break;

		case -ERESTARTSYS:
			/*
			 * ERESTARTSYS means to restart the syscall if
			 * there is no handler or the handler was
			 * registered with SA_RESTART
			 */
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->r0 = -EINTR;
				break;
			}
			/* fallthrough */

		case -ERESTARTNOINTR:
			/*
			 * ERESTARTNOINTR means that the syscall should
			 * be called again after the signal handler returns.
			 * Setup reg state just as it was before doing the trap
			 * r0 has been clobbered with sys call ret code thus it
			 * needs to be reloaded with orig first arg to syscall
			 * in orig_r0. Rest of relevant reg-file:
			 * r8 (syscall num) and (r1 - r7) will be reset to
			 * their orig user space value when we ret from kernel
			 */
			regs->r0 = regs->orig_r0;
			regs->ret -= 4;
			break;
		}
	}

	if (thread->exec_domain && thread->exec_domain->signal_invmap
	    && usig < 32)
		usig = thread->exec_domain->signal_invmap[usig];

	/* Set up the stack frame */
	ret = setup_rt_frame(usig, ka, info, oldset, regs);

	if (ret) {
		force_sigsegv(sig, current);
		return ret;
	}

	/* Block the signal if we are successful */
	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked, &current->blocked, &ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked, sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	return ret;
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
void do_signal(struct pt_regs *regs)
{
	struct k_sigaction ka;
	siginfo_t info;
	int signr;
	sigset_t *oldset;
	int insyscall;

	if (try_to_freeze())
		goto no_signal;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);

	/* Are we from a system call? */
	insyscall = in_syscall(regs);

	if (signr > 0) {
		if (handle_signal(signr, &ka, &info, oldset, regs, insyscall) ==
		    0) {
			/*
			 * A signal was successfully delivered; the saved
			 * sigmask will have been stored in the signal frame,
			 * and will be restored by sigreturn, so we can simply
			 * clear the TIF_RESTORE_SIGMASK flag.
			 */
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
		}
		return;
	}

no_signal:
	if (insyscall) {
		/* No handler for syscall: restart it */
		if (regs->r0 == -ERESTARTNOHAND ||
		    regs->r0 == -ERESTARTSYS || regs->r0 == -ERESTARTNOINTR) {
			regs->r0 = regs->orig_r0;
			regs->ret -= 4;
		} else if (regs->r0 == -ERESTART_RESTARTBLOCK) {
			regs->r8 = __NR_restart_syscall;
			regs->ret -= 4;
		}
	}

	/*
	 * If there's no signal to deliver, restore the saved sigmask back
	 */
	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

static noinline void
mod_tlb_permission(unsigned long frame_vaddr, struct mm_struct *mm,
		   int exec_on)
{
	unsigned int cpu = smp_processor_id();
	unsigned long frame_tlbpd1, flags;
	unsigned int asid;

	if (!mm)
		return;
	local_irq_save(flags);

	asid = mm->context.asid[cpu];

	frame_vaddr = frame_vaddr & PAGE_MASK;
	/* get the ASID */
	frame_vaddr = frame_vaddr | (asid & 0xff);
	write_aux_reg(ARC_REG_TLBPD0, frame_vaddr);
	write_aux_reg(ARC_REG_TLBCOMMAND, TLBProbe);

	if (read_aux_reg(ARC_REG_TLBINDEX) != TLB_LKUP_ERR) {
		write_aux_reg(ARC_REG_TLBCOMMAND, TLBRead);
		frame_tlbpd1 = read_aux_reg(ARC_REG_TLBPD1);

		if (!exec_on) {
			/* disable execute permissions for the user stack */
			frame_tlbpd1 = frame_tlbpd1 & ~_PAGE_EXECUTE;
		} else {
			/* enable execute */
			frame_tlbpd1 = frame_tlbpd1 | _PAGE_EXECUTE;
		}

		write_aux_reg(ARC_REG_TLBPD1, frame_tlbpd1);
		write_aux_reg(ARC_REG_TLBCOMMAND, TLBWrite);
	}

	local_irq_restore(flags);
}

static noinline void
set_frame_exec(unsigned long vaddr, unsigned int exec_on)
{
	unsigned long paddr, vaddr_pg, off_from_pg_start;
	pgd_t *pgdp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep, pte;
	unsigned long size_on_pg, size_on_pg2;
	unsigned long fr_sz = sizeof(((struct sigframe *) (0))->retcode);

	off_from_pg_start = vaddr - (vaddr & PAGE_MASK);
	size_on_pg = min(fr_sz, PAGE_SIZE - off_from_pg_start);
	size_on_pg2 = fr_sz - size_on_pg;

do_per_page:

	vaddr_pg = vaddr & PAGE_MASK;	/* Get the virtual page address */

	/* Get the physical page address for the virtual page address */
	pgdp = pgd_offset_fast(current->mm, vaddr_pg),
	    pudp = pud_offset(pgdp, vaddr_pg);
	pmdp = pmd_offset(pudp, vaddr_pg);
	ptep = pte_offset(pmdp, vaddr_pg);

	/* Set the Execution Permission in the pte entry */
	pte = *ptep;
	if (exec_on)
		pte = pte_mkexec(pte);
	else
		pte = pte_exprotect(pte);
	set_pte(ptep, pte);

	/* Get the physical page address */
	paddr = (vaddr & ~PAGE_MASK) | (pte_val(pte) & PAGE_MASK);

	/* Flush dcache line, and inv Icache line for frame->retcode */
	flush_icache_range_vaddr(paddr, vaddr, size_on_pg);

	mod_tlb_permission(vaddr_pg, current->mm, exec_on);

	if (size_on_pg2) {
		vaddr = vaddr_pg + PAGE_SIZE;
		size_on_pg = size_on_pg2;
		size_on_pg2 = 0;
		goto do_per_page;
	}
}
