/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 * Vineetg: Oct 2009
 *  No need for ARC specific thread_info allocator (kmalloc/free). This is
 *  anyways one page allocation, thus slab alloc can be short-circuited and
 *  the generic version (get_free_page) would be loads better.
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com)
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 *  linux/include/asm-arc/thread_info.h
 *
 *  Copyright (C)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Authors: Sameer Dhavale
 */

/*
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H

#ifdef __KERNEL__

#include <asm/page.h>

#define THREAD_SIZE_ORDER 1
#ifdef CONFIG_16KSTACKS
#define THREAD_SIZE     (PAGE_SIZE << 1)
#else
#define THREAD_SIZE     PAGE_SIZE
#endif

#ifndef __ASSEMBLY__

#include <linux/linkage.h>
#include <linux/thread_info.h>


typedef unsigned long mm_segment_t;     /* domain register  */

/*
 * low level task data that entry.S needs immediate access to
 * - this struct should fit entirely inside of one cache line
 * - this struct shares the supervisor stack pages
 * - if the contents of this structure are changed, the assembly constants
 *   must also be changed
 */
struct thread_info {
    struct task_struct  *task;      /* main task structure */
    struct exec_domain  *exec_domain;   /* execution domain */
    unsigned long       flags;      /* low level flags */
    unsigned long       tp_value;   /* thread pointer */
    __u32           cpu;        /* current CPU */
    int         preempt_count;  /* 0 => preemptable, <0 => BUG */

    mm_segment_t        addr_limit; /* thread address space:
                           0-0xBFFFFFFF for user-thead
                           0-0xFFFFFFFF for kernel-thread
                        */
    struct restart_block    restart_block;
    /* struct pt_regs       *regs; */
};

/*
 * macros/functions for gaining access to the thread information structure
 *
 * preempt_count needs to be 1 initially, until the scheduler is functional.
 */
#define INIT_THREAD_INFO(tsk)           \
{                       \
    .task       = &tsk,         \
    .exec_domain    = &default_exec_domain, \
    .flags      = 0,            \
    .cpu        = 0,            \
    .preempt_count  = 1,            \
    .addr_limit = KERNEL_DS,        \
    .restart_block  = {         \
        .fn = do_no_restart_syscall,    \
    },                  \
}

#define init_thread_info    (init_thread_union.thread_info)
#define init_stack          (init_thread_union.stack)

static inline struct thread_info *current_thread_info(void) __attribute_const__;

static inline struct thread_info *current_thread_info(void)
{
    register unsigned long sp asm ("sp");
    return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}

#else /*  __ASSEMBLY__ */

.macro GET_CURR_THR_INFO_FROM_SP  reg
    and \reg, sp, ~(THREAD_SIZE - 1)
.endm

#endif  /* !__ASSEMBLY__ */


#define PREEMPT_ACTIVE      0x10000000

/*
 * thread information flags
 * - these are process state flags that various assembly files may need to
 *   access
 * - pending work-to-be-done flags are in LSW
 * - other flags in MSW
 */
#define TIF_NOTIFY_RESUME   1   /* resumption notification requested */
#define TIF_SIGPENDING      2   /* signal pending */
#define TIF_NEED_RESCHED    3   /* rescheduling necessary */
#define TIF_SYSCALL_AUDIT   4   /* syscall auditing active */
#define TIF_SECCOMP     5   /* secure computing */
#define TIF_RESTORE_SIGMASK 9   /* restore signal mask in do_signal() */
#define TIF_USEDFPU     16  /* FPU was used by this task this quantum (SMP) */
#define TIF_POLLING_NRFLAG  17  /* true if poll_idle() is polling TIF_NEED_RESCHED */
#define TIF_MEMDIE      18
#define TIF_SYSCALL_TRACE   31  /* syscall trace active */

#define _TIF_SYSCALL_TRACE  (1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME  (1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING     (1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED   (1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT  (1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP        (1<<TIF_SECCOMP)
#define _TIF_RESTORE_SIGMASK    (1<<TIF_RESTORE_SIGMASK)
#define _TIF_USEDFPU        (1<<TIF_USEDFPU)
#define _TIF_POLLING_NRFLAG (1<<TIF_POLLING_NRFLAG)

/* work to do on interrupt/exception return */
#define _TIF_WORK_MASK      (0x0000ffef & ~_TIF_SECCOMP)
/* work to do on any return to u-space */
#define _TIF_ALLWORK_MASK   (0x8000ffff & ~_TIF_SECCOMP)

#endif /* __KERNEL__ */

#endif /* _ASM_THREAD_INFO_H */
