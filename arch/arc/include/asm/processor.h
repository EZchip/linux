/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 *
 * vineetg: March 2009
 *  -Implemented task_pt_regs( )
 *
 *
 *
 *****************************************************************************/

/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 *  linux/include/asm-arc/processor.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors : Amit Bhor, Sameer Dhavale
 * Ashwin Chaugule <ashwin.chaugule@codito.com>
 * Added additional cpuinfo for /proc/cpuinfo
 */

#ifndef __ASM_ARC_PROCESSOR_H
#define __ASM_ARC_PROCESSOR_H

#include <linux/linkage.h>

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 * Should the PC register be read instead ? This macro does not seem to
 * be used in many places so this wont be all that bad.
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#ifdef __KERNEL__

#include <asm/page.h>       /* for PAGE_OFFSET  */
#include <asm/arcregs.h>    /* for STATUS_E1_MASK et all */

/* Most of the architectures seem to be keeping some kind of padding between
 * userspace TASK_SIZE and PAGE_OFFSET. i.e TASK_SIZE != PAGE_OFFSET.
 * I'm not sure why it is so. We will keep this padding for now and remove it
 * later if not required.
 * 256 MB so last user addr will be 0x5FFF_FFFF
 */
#define ARC_SOME_PADDING    0x10000000

/* User space process size:
 * Untraslated start - kernel VM size (CONFIG_VMALLOC_SIZE) - ARC_SOME_PADDING.
 */
#define TASK_SIZE   (PAGE_OFFSET - (CONFIG_VMALLOC_SIZE * 1024 * 1024) - ARC_SOME_PADDING)

#define STACK_TOP_MAX TASK_SIZE

#ifdef __KERNEL__
#define STACK_TOP TASK_SIZE
#endif


/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 3)

/* For mmap randomisation and Page coloring for share code pages */
#define HAVE_ARCH_PICK_MMAP_LAYOUT

/* Arch specific stuff which needs to be saved per task.
 * However these items are not so important so as to earn a place in
 * struct thread_info
 */
struct thread_struct {
    unsigned long   ksp;            /* kernel mode stack pointer */
    unsigned long   callee_reg;     /* pointer to callee regs */
    unsigned long   fault_address;  /* fault address when exception occurs */
    unsigned long   cause_code;     /* Exception Cause Code (ECR) */
#ifdef CONFIG_ARCH_ARC_CURR_IN_REG
    unsigned long   user_r25;
#endif
};

#define INIT_THREAD  {                          \
    .ksp = sizeof(init_stack) + (unsigned long) init_stack, \
}

/* Forward declaration, a strange C thing */
struct task_struct;

/*
 * Return saved PC of a blocked thread.
 */
unsigned long thread_saved_pc(struct task_struct *t);

#define task_pt_regs(p) \
	((struct pt_regs *)(THREAD_SIZE - 4 + (void *)task_stack_page(p)) - 1)

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while(0)

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk)    do { } while (0)

#define cpu_relax()    do { } while (0)

/*
 * Create a new kernel thread
 */


extern int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

#define copy_segments(tsk, mm)      do { } while (0)
#define release_segments(mm)        do { } while (0)

#define KSTK_EIP(tsk)   (task_pt_regs(tsk)->ret)

/* Where abouts of Task's sp, fp, blink when it was last seen in kernel mode.
 * So these can't be derived from pt_regs as that would give it's
 * sp, fp, blink of user mode
 */
#define KSTK_ESP(tsk)   (tsk->thread.ksp)
#define KSTK_BLINK(tsk) (*((unsigned int *)((KSTK_ESP(tsk)) + (13+1+1)*4)))
#define KSTK_FP(tsk)    (*((unsigned int *)((KSTK_ESP(tsk)) + (13+1)*4)))

/*
 * Do necessary setup to start up a newly executed thread.
 *
 * E1,E2 so that Interrupts are enabled in user mode
 * L set, so Loop inhibited to begin with
 * lp_start and lp_end seeded with bogus non-zero values so to easily catch
 * the ARC700 sr to lp_start hardware bug
 */
#define start_thread(_regs, _pc, _usp)          \
do {                            \
    set_fs(USER_DS); /* reads from user space */    \
    (_regs)->ret = (_pc);               \
    /* User mode, E1 and E2 enabled */      \
    (_regs)->status32 = STATUS_U_MASK       \
                 | STATUS_L_MASK  \
                 | STATUS_E1_MASK   \
                 | STATUS_E2_MASK;  \
    (_regs)->sp = (_usp);                   \
    (_regs)->lp_start = 0x10;                   \
    (_regs)->lp_end = 0x80;                   \
} while(0)

extern unsigned int get_wchan(struct task_struct *p);

#endif  /* __KERNEL__ */
#endif  /* __ASM_ARC_PROCESSOR_H */
